#include "dip_opencv_interface.h"
namespace dcv = dip_opencv;

#include "diplib/viewer/glfw.h"
#include "diplib/viewer/slice.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <unistd.h>

class CaptureThread
{
   protected:
      cv::VideoCapture &cap_;
      cv::Mat frame_;
      bool updated_;
      bool continue_;
      std::thread thread_;
      
   public:
      CaptureThread( cv::VideoCapture &cap ) : cap_( cap ), updated_( false ), continue_( true ) {
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
      
      cv::Mat frame() {
         return frame_;
      }
  
   protected:
      void run() {
         while ( continue_ ) {
            if ( !cap_.read( frame_ ) ) {
               std::cout << "Cannot capture frame" << std::endl;
               
               continue_ = false;
               break;
            }

            updated_ = true;
         }
         
      }
};

int main(int argc, char* argv[])
{
   cv::VideoCapture cap( 0 );

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
   
   CaptureThread *thread = new CaptureThread( cap );
   
   // Wait for first frame
   while ( !thread->updated() ) {
      // Bail on error
      if ( !thread->running() ) {
         return 1;
      }
   
      usleep( 1000 );
   }
   
   // Convert OpenCV frame to DIPlib. As the memory will be reused, we do this
   // only once.
   dip::Image frame = dcv::MatToDip( thread->frame() );
   
   // Convert BGR to RGB
   frame.TensorToSpatial( 2 );
   frame.Mirror( { 0, 0, 1 } );
   frame.SpatialToTensor( 2 );
   frame.SetColorSpace( "RGB" );

   // Show image      
   dip::viewer::SliceViewer::Ptr window = dip::viewer::SliceViewer::Create( frame, "Webcam" );
   dip::viewer::GLFWManager manager;
   manager.createWindow( window );

   // Update image when new frames arrive, but allow continuous interaction.
   while ( manager.activeWindows() ) {
      if ( thread->updated() )
      {
         dip::viewer::Viewer::Guard guard( *window );
         window->setImage( frame );
      }

      manager.processEvents();
      usleep( 1000 );
   }

   delete thread;   
   cap.release();

   return 0;
}
