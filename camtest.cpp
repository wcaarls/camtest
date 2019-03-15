#include "dip_opencv_interface.h"
namespace dcv = dip_opencv;

#include "diplib/viewer/glfw.h"
#include "diplib/viewer/slice.h"

#include <opencv2/opencv.hpp>
#include <iostream>

size_t framecnt = 0, showcnt = 0;

void run( cv::VideoCapture &cap, dip::Image &frame ) {
  cv::Mat cvframe;

  while (1) {
     while ( showcnt != framecnt ) {
        usleep(1000);
     }
  
     if ( !cap.read( cvframe) ) {
        break;
     }
     
     // Directly capturing to a shared data segment
     // results in memory corruption. Not sure why.
     frame = dcv::MatToDip( cvframe ).Copy();
     framecnt++;
  }
}

int main(int argc, char* argv[])
{
   cv::VideoCapture cap(0);

   if ( !cap.isOpened() ) {
      std::cout << "Cannot open the camera" << std::endl;
      return 1;
   }
   
   if ( argc > 2 ) {
      cap.set( cv::CAP_PROP_FRAME_WIDTH, atoi( argv[ 1 ] ) );
      cap.set( cv::CAP_PROP_FRAME_HEIGHT, atoi (argv[ 2 ] ) );
   }

   if ( argc > 3 ) {
      cap.set( cv::CAP_PROP_FOURCC, CV_FOURCC( argv[3][0], argv[3][1], argv[3][2], argv[3][3] ));
   }
   
   // Start capturing images continually
   dip::Image frame;
   std::thread thread = std::thread( run, std::ref( cap ), std::ref( frame ) );
   
   // Get first frame
   while ( !framecnt )
      usleep(1000);
      
   std::cout << frame;

   // Show image      
   dip::viewer::SliceViewer::Ptr window = dip::viewer::SliceViewer::Create( frame, "Webcam" );
   dip::viewer::GLFWManager manager;
   manager.createWindow( window );

   while ( manager.activeWindows() ) {
      if ( showcnt != framecnt ) {
         dip::viewer::Viewer::Guard guard( *window );
         window->setImage( frame );
         showcnt = framecnt;
      }

      manager.processEvents();
   }

   // Wait for last capture to be done
   while ( showcnt == framecnt )
      usleep(1000);

   cap.release();
   
   showcnt++;
   thread.join();

   return 0;
}
