// See: https://www.kernel.org/doc/html/v4.11/media/uapi/v4l/v4l2grab.c.html
// See: https://stackoverflow.com/questions/9094691/examples-or-tutorials-of-using-libjpeg-turbos-turbojpeg

#include "diplib/viewer/glfw.h"
#include "diplib/viewer/slice.h"

#include <iostream>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>

#include <turbojpeg.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
   void   *start;
   size_t length;
};

static void xioctl(int fh, int request, void *arg)
{
   int r;

   do {
      r = v4l2_ioctl(fh, request, arg);
   } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

   if (r == -1) {
      fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
      exit(EXIT_FAILURE);
   }
}

class __attribute__ ((visibility("hidden"))) CaptureThread
{
   protected:
      int fd_;
      dip::Image image_;
      bool updated_;
      bool continue_;
      std::thread thread_;
      
   public:
      CaptureThread( int fd, dip::Image &image ) : fd_(fd), image_(image), updated_( false ), continue_( true ) {
         thread_ = std::thread( &CaptureThread::run, this );
      }
      
      ~CaptureThread() {
         continue_ = false;
         thread_.join();
      }
      
      // NOTE: will miss some frames due to race conditions
      bool updated() {
         if ( updated_ ) {
            updated_ = false;
            return true;
         } else {
            return false;
         }
      }
      
      bool running() {
         return continue_;
      }
      
   protected:
      void run() {
         // V4L init
         struct v4l2_buffer              buf;
         struct v4l2_requestbuffers      req;
         enum v4l2_buf_type              type;
         fd_set                          fds;
         struct timeval                  tv;
         int                             r;
         unsigned int                    i, n_buffers;
         struct buffer                   *buffers;

         CLEAR(req);
         req.count = 2;
         req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         req.memory = V4L2_MEMORY_MMAP;
         xioctl(fd_, VIDIOC_REQBUFS, &req);

         buffers = (struct buffer*) calloc(req.count, sizeof(*buffers));
         for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
            CLEAR(buf);
 
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = n_buffers;
 
            xioctl(fd_, VIDIOC_QUERYBUF, &buf);
 
            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
                          PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd_, buf.m.offset);
 
            if (MAP_FAILED == buffers[n_buffers].start) {
                    perror("mmap");
                    exit(EXIT_FAILURE);
            }
         }
 
         for (i = 0; i < n_buffers; ++i) {
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            xioctl(fd_, VIDIOC_QBUF, &buf);
         }
         type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         xioctl(fd_, VIDIOC_STREAMON, &type);
         
         // Avoid "libv4l2: error dequeuing buf: Resource temporarily unavailable"
         usleep(1000000);
     
         // JPEG init
         tjhandle jpegDecompressor = tjInitDecompress();
         
         while ( continue_ ) {
            // V4L capture
            do {
               FD_ZERO(&fds);
               FD_SET(fd_, &fds);

               /* Timeout. */
               tv.tv_sec = 2;
               tv.tv_usec = 0;

               r = select(fd_ + 1, &fds, NULL, NULL, &tv);
            } while ((r == -1 && (errno = EINTR)));
            if (r == -1) {
               perror("select");
               break;
            }

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            xioctl(fd_, VIDIOC_DQBUF, &buf);
            
            // JPEG decompression
            unsigned char *compressedImage = (unsigned char*) buffers[buf.index].start;
            unsigned long jpegSize = buf.bytesused;

            int jpegColorspace, jpegSubsamp, width, height;
            tjDecompressHeader3(jpegDecompressor, compressedImage, jpegSize, &width, &height, &jpegSubsamp, &jpegColorspace);
            
            if (width == image_.Size(0) && height == image_.Size(1))
              tjDecompress2(jpegDecompressor, compressedImage, jpegSize, (unsigned char*) image_.Origin(), width, 0/*pitch*/, height, TJPF_RGB, 0);

            xioctl(fd_, VIDIOC_QBUF, &buf);
            updated_ = true;
         }
         
         // V4L deinit
         type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         xioctl(fd_, VIDIOC_STREAMOFF, &type);
         for (i = 0; i < n_buffers; ++i)
            v4l2_munmap(buffers[i].start, buffers[i].length);
         
         // JPEG deinit
         tjDestroy(jpegDecompressor);
         
         continue_ = false;
         updated_ = true;
      }
};

int main(int argc, char* argv[])
{
   int width=640, height=480;
   
   if ( argc > 2 ) {
      width = atoi( argv[ 1 ] );
      height = atoi (argv[ 2 ] );
   }

   int fd = v4l2_open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
   if (fd < 0) {
      perror("Cannot open device");
      exit(EXIT_FAILURE);
   }

   struct v4l2_format fmt;
   CLEAR(fmt);
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width       = width;
   fmt.fmt.pix.height      = height;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
   fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
   xioctl(fd, VIDIOC_S_FMT, &fmt);
   if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
      printf("Libv4l didn't accept MJPEG format. Can't proceed.\n");
      return 1;
   }
   if ((fmt.fmt.pix.width != width) || (fmt.fmt.pix.height != height))
   {
      printf("Warning: driver is sending image at %dx%d.\n",
              fmt.fmt.pix.width, fmt.fmt.pix.height);
      width = fmt.fmt.pix.width;
      height = fmt.fmt.pix.height;
   }
   
   dip::Image image({(dip::uint)width, (dip::uint)height}, 3, dip::DT_UINT8);
   image.SetColorSpace( "RGB" );

   CaptureThread *thread = new CaptureThread( fd, image );
   
   // Wait for first frame
   while ( !thread->updated() ) {
      // Bail on error
      if ( !thread->running() ) {
         return 1;
      }
   
      usleep( 1000 );
   }

   // Show image      
   dip::viewer::SliceViewer::Ptr window = dip::viewer::SliceViewer::Create( image, "Webcam" );
   dip::viewer::GLFWManager manager;
   manager.createWindow( window );

   // Update image when new frames arrive, but allow continuous interaction.
   while ( manager.activeWindows() ) {
      if ( thread->updated() )
      {
         dip::viewer::Viewer::Guard guard( *window );
         window->setImage( image );
      }

      manager.processEvents();
      usleep( 1000 );
   }

   delete thread;   

   v4l2_close(fd);

   return 0;
}
