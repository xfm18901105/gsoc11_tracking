/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include "object_tracker.h"
#include <iostream>
#include <cstdlib>

namespace cv
{

  //---------------------------------------------------------------------------
  ObjectTrackerParams::ObjectTrackerParams()
  {
    // By default, use online boosting
    algorithm_ = CV_ONLINEBOOSTING;

    // Default number of classifiers/selectors for boosting algorithms
    num_classifiers_ = 100;

    // Default search region parameters for boosting algorithms
    overlap_ = 0.99f;
    search_factor_ = 2.0f;
  }

  //---------------------------------------------------------------------------
  ObjectTrackerParams::ObjectTrackerParams(const int algorithm, const int num_classifiers,
    const float overlap, const float search_factor)
  {
    // Make sure a valid algorithm flag is used before storing it
    if ( (algorithm != CV_ONLINEBOOSTING) && (algorithm != CV_SEMIONLINEBOOSTING) && (algorithm != CV_ONLINEMIL) && (algorithm != CV_LINEMOD) )
    {
      // Use CV_ERROR?
      std::cerr << "ObjectTrackerParams::ObjectTrackerParams(...) -- ERROR!  Invalid algorithm choice.\n";
      exit(-1);
    }
    // Store it
    algorithm_ = algorithm;

    // Store the number of weak classifiers (any error checking done here?)
    num_classifiers_ = num_classifiers;

    // Store information about the searching 
    overlap_ = overlap;
    search_factor_ = search_factor;
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  TrackingAlgorithm::TrackingAlgorithm()
    : image_(NULL)
  {
  }

  //---------------------------------------------------------------------------
  TrackingAlgorithm::~TrackingAlgorithm()
  {
    if (image_ != NULL)
    {
      cvReleaseImage(&image_);
      image_ = NULL;
    }
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  OnlineBoostingAlgorithm::OnlineBoostingAlgorithm() 
    : TrackingAlgorithm(), tracker_(NULL), cur_frame_rep_(NULL)
  {
  }

  //---------------------------------------------------------------------------
  OnlineBoostingAlgorithm::~OnlineBoostingAlgorithm()
  {
    if (tracker_ != NULL)
    {
      delete tracker_;
      tracker_ = NULL;
    }

    if (cur_frame_rep_ != NULL)
    {
      delete cur_frame_rep_;
      cur_frame_rep_ = NULL;
    }
  }

  //---------------------------------------------------------------------------
  bool OnlineBoostingAlgorithm::initialize(const IplImage* image, const ObjectTrackerParams& params, 
    const CvRect& init_bounding_box)
  {
    // Import the image
    import_image(image);

    // If the boosting tracker has already been allocated, first de-allocate it
    if (tracker_ != NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::initialize(...) -- WARNING!  Boosting tracker already initialized.  Resetting now...\n" << std::endl;
      delete tracker_;
      tracker_ = NULL;
    }

    // Do the same for the image frame representation
    if (cur_frame_rep_ != NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::initialize(...) -- WARNING!  Boosting tracker already initialized.  Resetting now...\n" << std::endl;
      delete cur_frame_rep_;
      cur_frame_rep_ = NULL;
    }

    // (Re-)Initialize the boosting tracker
    boosting::Size imageSize(image_->height, image_->width);
    cur_frame_rep_ = new boosting::ImageRepresentation((unsigned char*)image_->imageData, imageSize);
    boosting::Rect wholeImage;
    wholeImage = imageSize;
    boosting::Rect tracking_rect = cvrect_to_rect(init_bounding_box);
    tracking_rect.confidence = 0;
    tracking_rect_size_ = tracking_rect;
    tracker_ = new boosting::BoostingTracker(cur_frame_rep_, tracking_rect, wholeImage, params.num_classifiers_);

    // Initialize some useful tracking debugging information
    tracker_lost_ = false;

    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  bool OnlineBoostingAlgorithm::update(const IplImage* image, const ObjectTrackerParams& params, 
    CvRect* track_box)
  {
    // Import the image
    import_image(image);

    // Make sure the tracker has already been successfully initialized
    if (tracker_ == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::update(...) -- ERROR!  Trying to call update without properly initializing the tracker!\n" << std::endl;
      return false;
    }
    if (cur_frame_rep_ == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::update(...) -- ERROR!  Trying to call update without properly initializing the tracker!\n" << std::endl;
      return false;
    }

    // Calculate the patches within the search region
    boosting::Size imageSize(image_->height, image_->width);
    boosting::Rect wholeImage;
    wholeImage = imageSize;
    boosting::Patches *trackingPatches;	
    boosting::Rect searchRegion;
    searchRegion = tracker_->getTrackingROI(params.search_factor_);
    trackingPatches = new boosting::PatchesRegularScan(searchRegion, wholeImage, tracking_rect_size_, params.overlap_);

    cur_frame_rep_->setNewImageAndROI((unsigned char*)image_->imageData, searchRegion);

    if (!tracker_->track(cur_frame_rep_, trackingPatches))
    {
      tracker_lost_ = true;
    }
    else
    {
      tracker_lost_ = false;
    }

    delete trackingPatches;

    // Save the new tracking ROI
    *track_box = rect_to_cvrect(tracker_->getTrackedPatch());
    std::cout << "\rTracking confidence = " << tracker_->getConfidence();

    // Return success or failure based on whether or not the tracker has been lost
    return !tracker_lost_;
  }

  //---------------------------------------------------------------------------
  void OnlineBoostingAlgorithm::import_image(const IplImage* image)
  {
    // We want the internal version of the image to be gray-scale, so let's
    // do that here.  We'll handle cases where the input is either RGB, RGBA,
    // or already gray-scale.  I assume it's already 8-bit.  If not then 
    // an error is thrown.  I'm not going to deal with converting properly
    // from every data type since that shouldn't be happening.

    // Make sure the input image pointer is valid
    if (image == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::import_image(...) -- ERROR!  Input image pointer is NULL!\n" << std::endl;
      exit(0);  // <--- CV_ERROR?
    }

    // First, make sure our image is allocated
    if (image_ == NULL)
    {
      image_ = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 1);
    }

    // Now copy it in appropriately as a gray-scale, 8-bit image
    if (image->nChannels == 4)
    {
      cvCvtColor(image, image_, CV_RGBA2GRAY);
    }
    else if (image->nChannels == 3)
    {
      cvCvtColor(image, image_, CV_RGB2GRAY);
    }
    else if (image->nChannels == 1)
    {
      cvCopy(image, image_);
    }
    else
    {
      std::cerr << "OnlineBoostingAlgorithm::import_image(...) -- ERROR!  Invalid number of channels for input image!\n" << std::endl;
      exit(0);
    }
  }

  //---------------------------------------------------------------------------
  boosting::Rect OnlineBoostingAlgorithm::cvrect_to_rect(const CvRect& in)
  {
    return boosting::Rect(in.y, in.x, in.height, in.width);
  }

  //---------------------------------------------------------------------------
  CvRect OnlineBoostingAlgorithm::rect_to_cvrect(const boosting::Rect& in)
  {
    return cvRect(in.left, in.upper, in.width, in.height);
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  SemiOnlineBoostingAlgorithm::SemiOnlineBoostingAlgorithm() 
    : TrackingAlgorithm(), tracker_(NULL), cur_frame_rep_(NULL)
  {
  }

  //---------------------------------------------------------------------------
  SemiOnlineBoostingAlgorithm::~SemiOnlineBoostingAlgorithm()
  {
    if (tracker_ != NULL)
    {
      delete tracker_;
      tracker_ = NULL;
    }

    if (cur_frame_rep_ != NULL)
    {
      delete cur_frame_rep_;
      cur_frame_rep_ = NULL;
    }
  }

  //---------------------------------------------------------------------------
  bool SemiOnlineBoostingAlgorithm::initialize(const IplImage* image, const ObjectTrackerParams& params, 
    const CvRect& init_bounding_box)
  {
    // Import the image
    import_image(image);

    // If the boosting tracker has already been allocated, first de-allocate it
    if (tracker_ != NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::initialize(...) -- WARNING!  Boosting tracker already initialized.  Resetting now...\n" << std::endl;
      delete tracker_;
      tracker_ = NULL;
    }

    // Do the same for the image frame representation
    if (cur_frame_rep_ != NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::initialize(...) -- WARNING!  Boosting tracker already initialized.  Resetting now...\n" << std::endl;
      delete cur_frame_rep_;
      cur_frame_rep_ = NULL;
    }

    // (Re-)Initialize the boosting tracker
    boosting::Size imageSize(image_->height, image_->width);
    cur_frame_rep_ = new boosting::ImageRepresentation((unsigned char*)image_->imageData, imageSize);
    boosting::Rect wholeImage;
    wholeImage = imageSize;
    boosting::Rect tracking_rect = cvrect_to_rect(init_bounding_box);
    tracking_rect.confidence = 0;
    tracking_rect_size_ = tracking_rect;
    tracker_ = new boosting::SemiBoostingTracker(cur_frame_rep_, tracking_rect, wholeImage, params.num_classifiers_);

    // Initialize some useful tracking debugging information
    tracker_lost_ = false;

    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  bool SemiOnlineBoostingAlgorithm::update(const IplImage* image, const ObjectTrackerParams& params, 
    CvRect* track_box)
  {
    // Import the image
    import_image(image);

    // Make sure the tracker has already been successfully initialized
    if (tracker_ == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::update(...) -- ERROR!  Trying to call update without properly initializing the tracker!\n" << std::endl;
      return false;
    }
    if (cur_frame_rep_ == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::update(...) -- ERROR!  Trying to call update without properly initializing the tracker!\n" << std::endl;
      return false;
    }

    // Calculate the patches within the search region
    boosting::Size imageSize(image_->height, image_->width);
    boosting::Rect wholeImage;
    wholeImage = imageSize;
    boosting::Patches *trackingPatches;	
    boosting::Rect searchRegion;
    searchRegion = tracker_->getTrackingROI(params.search_factor_);
    trackingPatches = new boosting::PatchesRegularScan(searchRegion, wholeImage, tracking_rect_size_, params.overlap_);

    cur_frame_rep_->setNewImageAndROI((unsigned char*)image_->imageData, searchRegion);

    if (!tracker_->track(cur_frame_rep_, trackingPatches))
    {
      tracker_lost_ = true;
    }
    else
    {
      tracker_lost_ = false;
    }

    delete trackingPatches;

    // Save the new tracking ROI
    *track_box = rect_to_cvrect(tracker_->getTrackedPatch());
    std::cout << "\rTracking confidence = " << tracker_->getConfidence();

    // Return success or failure based on whether or not the tracker has been lost
    return !tracker_lost_;
  }

  //---------------------------------------------------------------------------
  void SemiOnlineBoostingAlgorithm::import_image(const IplImage* image)
  {
    // We want the internal version of the image to be gray-scale, so let's
    // do that here.  We'll handle cases where the input is either RGB, RGBA,
    // or already gray-scale.  I assume it's already 8-bit.  If not then 
    // an error is thrown.  I'm not going to deal with converting properly
    // from every data type since that shouldn't be happening.

    // Make sure the input image pointer is valid
    if (image == NULL)
    {
      std::cerr << "OnlineBoostingAlgorithm::import_image(...) -- ERROR!  Input image pointer is NULL!\n" << std::endl;
      exit(0);  // <--- CV_ERROR?
    }

    // First, make sure our image is allocated
    if (image_ == NULL)
    {
      image_ = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 1);
    }

    // Now copy it in appropriately as a gray-scale, 8-bit image
    if (image->nChannels == 4)
    {
      cvCvtColor(image, image_, CV_RGBA2GRAY);
    }
    else if (image->nChannels == 3)
    {
      cvCvtColor(image, image_, CV_RGB2GRAY);
    }
    else if (image->nChannels == 1)
    {
      cvCopy(image, image_);
    }
    else
    {
      std::cerr << "OnlineBoostingAlgorithm::import_image(...) -- ERROR!  Invalid number of channels for input image!\n" << std::endl;
      exit(0);
    }
  }

  //---------------------------------------------------------------------------
  boosting::Rect SemiOnlineBoostingAlgorithm::cvrect_to_rect(const CvRect& in)
  {
    return boosting::Rect(in.y, in.x, in.height, in.width);
  }

  //---------------------------------------------------------------------------
  CvRect SemiOnlineBoostingAlgorithm::rect_to_cvrect(const boosting::Rect& in)
  {
    return cvRect(in.left, in.upper, in.width, in.height);
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  OnlineMILAlgorithm::OnlineMILAlgorithm() 
    : TrackingAlgorithm()
  {
  }

  //---------------------------------------------------------------------------
  OnlineMILAlgorithm::~OnlineMILAlgorithm()
  {
  }

  //---------------------------------------------------------------------------
  bool OnlineMILAlgorithm::initialize(const IplImage* image, const ObjectTrackerParams& params, 
    const CvRect& init_bounding_box)
  {
    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  bool OnlineMILAlgorithm::update(const IplImage* image, const ObjectTrackerParams& params, 
    CvRect* track_box)
  {
    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  void OnlineMILAlgorithm::import_image(const IplImage* image)
  {
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  LINEMODAlgorithm::LINEMODAlgorithm() 
    : TrackingAlgorithm()
  {
  }

  //---------------------------------------------------------------------------
  LINEMODAlgorithm::~LINEMODAlgorithm()
  {
  }

  //---------------------------------------------------------------------------
  bool LINEMODAlgorithm::initialize(const IplImage* image, const ObjectTrackerParams& params, 
    const CvRect& init_bounding_box)
  {
    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  bool LINEMODAlgorithm::update(const IplImage* image, const ObjectTrackerParams& params, 
    CvRect* track_box)
  {
    // Return success
    return true;
  }

  //---------------------------------------------------------------------------
  void LINEMODAlgorithm::import_image(const IplImage* image)
  {
  }

  //
  //
  //

  //---------------------------------------------------------------------------
  ObjectTracker::ObjectTracker(const ObjectTrackerParams& params)
    : initialized_(false), tracker_(NULL)
  {
    // Store configurable parameters internally
    set_params(params);

    // Allocate the proper tracking algorithm (note: error-checking that a valid
    // tracking algorithm parameter is used is done in the ObjectTrackerParams
    // constructor, so at this point we are confident it's valid).
    switch(params.algorithm_)
    {
    case ObjectTrackerParams::CV_ONLINEBOOSTING:
      tracker_ = new OnlineBoostingAlgorithm();
      break;
    case ObjectTrackerParams::CV_SEMIONLINEBOOSTING:
      tracker_ = new SemiOnlineBoostingAlgorithm();
      break;
    case ObjectTrackerParams::CV_ONLINEMIL:
      tracker_ = new OnlineMILAlgorithm();
      break;
    case ObjectTrackerParams::CV_LINEMOD:
      tracker_ = new LINEMODAlgorithm();
      break;
    default:
      // By default, if an invalid choice somehow gets through lets use online boosting?
      // Or throw an error and don't continue?
      tracker_ = new OnlineBoostingAlgorithm();
      break;
    }
  }

  //---------------------------------------------------------------------------
  ObjectTracker::~ObjectTracker()
  {
    // Delete the tracking algorithm object, if it was allocated properly
    if (tracker_ != NULL)
    {
      delete tracker_;
    }
  }

  //---------------------------------------------------------------------------
  bool ObjectTracker::initialize(const IplImage* image, const CvRect& bounding_box)
  {
    // Initialize the tracker and if it works, set the flag that we're now initialized
    // to true so that update() can work properly.
    bool success = tracker_->initialize(image, tracker_params_, bounding_box);
    if (success)
    {
      initialized_ = true;
    }
    else
    {
      initialized_ = false;
    }

    // Return success or failure
    return success;
  }

  //---------------------------------------------------------------------------
  bool ObjectTracker::update(const IplImage* image, CvRect* track_box)
  {
    // First make sure we have already initialized.  Otherwise we can't continue.
    if (!initialized_)
    {
      std::cerr << "ObjectTracker::update() -- ERROR! The ObjectTracker needs to be initialized before updating.\n";
      return false;
    }

    // Update the tracker and return whether or not it succeeded
    bool success = tracker_->update(image, tracker_params_, track_box);

    // Return success or failure
    return success;
  }

  //---------------------------------------------------------------------------
  void ObjectTracker::reset()
  {
    //
    // Reset the internal state of the tracker: should we just delete and re-allocate
    // the tracking algorithm object, or do something different?  Do we even really
    // need this functionality?
    //
  }

  //---------------------------------------------------------------------------
  void ObjectTracker::set_params(const ObjectTrackerParams& params)
  {
    tracker_params_ = params;
  }

  //---------------------------------------------------------------------------
  void ObjectTracker::save( const char* filename) const
  {
  }

  //---------------------------------------------------------------------------
  bool ObjectTracker::load( const char* filename)
  {
    // Return success
    return true;
  }

}
