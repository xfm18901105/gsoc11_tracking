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
#include "cv_onlineboosting.h"

// B/c windows has to redefine min and max for some reason (different
// from std::)
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <math.h>
#include <iostream>

#define M_PI (3.14159265358979323846)
#define FLOAT_MAX 3.4e38f
#define FLOAT_MIN 1.2e-38f
#define sign(x) (((x)>=0)? 1.0f : -1.0f)

#include <stdio.h>


/****************************************************************************************\
COPYRIGHT NOTICE
----------------

The code has been derived from the following on-line boosting trackers library:
(http://www.vision.ee.ethz.ch/boostingTrackers/).

/****************************************************************************************/


namespace cv
{
  namespace boosting
  {

    Color::Color()
    {
      red = 255;
      blue = 255;
      green = 255;
    }

    Color::Color(int red, int green, int blue)
    {
      this->red = red;
      this->green = green;
      this->blue = blue;
    }

    Color::Color(int idx)
    {
      switch (idx%6)
      {
      case 0: red = 255; blue = 0; green = 0; break;
      case 1: red = 0; blue = 255; green = 0; break;
      case 2: red = 0; blue = 0; green = 255; break;
      case 3: red = 255; blue = 255; green = 0; break;
      case 4: red = 0; blue = 255; green = 255; break;
      case 5: red = 255; blue = 255; green = 255; break;
      default: red = 255; blue = 255; green = 255; break;
      }
    }

    Rect::Rect()
    {
    }

    Rect::Rect (int upper, int left, int height, int width)
    {
      this->upper = upper;
      this->left = left;
      this->height = height;
      this->width = width;
    }

    Rect Rect::operator+ (Point2D p)
    {
      Rect r_tmp;
      r_tmp.upper = upper+p.row;
      r_tmp.left = left+p.col;
      r_tmp.height = height;
      r_tmp.width = width;
      return r_tmp;
    }

    Rect Rect::operator+ (Rect r)
    {
      Rect r_tmp;
      r_tmp.upper = std::min(upper, r.upper);
      r_tmp.left = std::min(left, r.left);
      r_tmp.height = std::max(upper+height, r.upper + r.height) - r_tmp.upper;
      r_tmp.width = std::max(left+width, r.left+r.width) - r_tmp.left;
      return r_tmp;
    }

    Rect Rect::operator- (Point2D p)
    {
      Rect r_tmp;
      r_tmp.upper = upper-p.row;
      r_tmp.left = left-p.col;
      r_tmp.height = height;
      r_tmp.width = width;
      return r_tmp;
    }

    Rect Rect::operator* (float f)
    {
      Rect r_tmp;
      r_tmp.upper = (int)(upper-((float)height*f-height)/2);
      if (r_tmp.upper < 0) r_tmp.upper = 0;
      r_tmp.left = (int)(left-((float)width*f-width)/2);
      if (r_tmp.left < 0) r_tmp.left = 0;
      r_tmp.height = (int)(height*f);
      r_tmp.width = (int)(width*f);

      return r_tmp;
    }


    Rect Rect::operator = (Size s)
    {
      height = s.height;
      width = s.width;
      upper = 0;
      left = 0;
      return *this;
    }

    Rect Rect::operator = (Rect r)
    {
      height = r.height;
      width = r.width;
      upper = r.upper;
      left = r.left;
      return *this;
    }

    bool Rect::operator== (Rect r)
    {	
      return ((r.width == width) && (r.height == height) && (r.upper == upper) && (r.left == left));
    }

    bool Rect::isValid (Rect validROI)
    {
      return (upper >= validROI.upper) &&  (upper <= validROI.upper +validROI.height) &&
        (left >= validROI.left) && (left <= validROI.left +validROI.width) &&
        (upper+height >= validROI.upper) && (upper+height <=validROI.upper+validROI.height) &&
        (left+width >= validROI.left) && (left+width <= validROI.left +validROI.width );

    }

    int Rect::checkOverlap(Rect rect)
    {	
      int x = (left > rect.left) ? left : rect.left;
      int y = (upper > rect.upper) ? upper : rect.upper;
      int w = (left+width > rect.left+rect.width) ? rect.left+rect.width-x : left+width-x;
      int h = (upper+height > rect.upper+rect.height) ? rect.upper+rect.height-y : upper+height-y;
      if (w > 0 && h > 0)
        return w*h;
      return 0;
    }


    bool Rect::isDetection(Rect eval, unsigned char *labeledImg, int imgWidth)
    {
      bool isDet = false;
      unsigned char labelEval;
      unsigned char labelDet;

      labelEval = labeledImg[(eval.upper)*imgWidth+eval.left];
      labelDet = labeledImg[(upper)*imgWidth+left];

      if ((labelEval == labelDet) && (labelDet != 0))
      {
        isDet = true;
      }
      else
      {
        isDet = false;
      }

      return isDet;
    }

    CvRect Rect::getCvRect()
    {
      return cvRect(left, upper, width, height);
    }

    Size::Size()
    {
    }

    Size::Size(int height, int width)
    {
      this->height = height;
      this->width = width;
    }

    Size Size::operator= (Rect r)
    {
      height = r.height;
      width = r.width;
      return *this;
    }

    Size Size::operator= (Size s)
    {
      height = s.height;
      width = s.width;
      return *this;
    }

    Size Size::operator* (float f)
    {
      Size s_tmp;
      s_tmp.height = (int)(height*f);
      s_tmp.width = (int)(width *f);
      return s_tmp;
    }


    bool Size::operator== (Size s)
    {	
      return ((s.width == width) && (s.height == height));
    }

    int Size::getArea()
    {
      return height*width;
    }

    Point2D::Point2D ()
    {
    }

    Point2D::Point2D (int row, int col)
    {
      this->row = row;
      this->col = col;
    }

    Point2D Point2D::operator+ (Point2D p)
    {
      Point2D p_tmp;
      p_tmp.col = col+p.col;
      p_tmp.row = row+p.row;
      return p_tmp;
    }

    Point2D Point2D::operator- (Point2D p)
    {
      Point2D p_tmp;
      p_tmp.col = col-p.col;
      p_tmp.row = row-p.row;
      return p_tmp;
    }

    Point2D Point2D::operator= (Point2D p)
    {
      row = p.row;
      col = p.col;
      return *this;
    }

    Point2D Point2D::operator= (Rect r)
    {
      row = r.upper;
      col = r.left;
      return *this;
    }

    ImageRepresentation::ImageRepresentation(unsigned char* image, Size imageSize)
    {  
      // call the default initialization
      this->defaultInit(image, imageSize);
      return;
    }

    ImageRepresentation::ImageRepresentation(unsigned char* image, Size imageSize, Rect imageROI)
    {
      this->m_imageSize = imageSize;

      m_ROI = imageROI;
      m_offset = m_ROI;

      m_useVariance = false;

      intImage = NULL;
      intSqImage = NULL;
      intImage = new __uint32[(m_ROI.width+1)*(m_ROI.height+1)];
      intSqImage = new __uint64[(m_ROI.width+1)*(m_ROI.height+1)];

      if (image!= NULL)
        this->createIntegralsOfROI(image);
    }


    void ImageRepresentation::defaultInit(unsigned char* image, Size imageSize)
    {  
      this->m_imageSize = imageSize;

      m_useVariance = false;

      m_ROI.height = imageSize.height;
      m_ROI.width = imageSize.width;
      m_ROI.upper = 0;
      m_ROI.left = 0;
      m_offset = m_ROI;

      intImage = NULL;
      intSqImage = NULL;


      intImage = new __uint32[(m_ROI.width+1)*(m_ROI.height+1)];
      intSqImage = new __uint64[(m_ROI.width+1)*(m_ROI.height+1)];

      if (image != NULL)
        this->createIntegralsOfROI(image);

      return;
    }

    ImageRepresentation::~ImageRepresentation()
    {

      delete[] intImage;
      delete[] intSqImage;
    }

    void ImageRepresentation::setNewImage(unsigned char* image)
    {
      createIntegralsOfROI(image);
    }

    void ImageRepresentation::setNewROI(Rect ROI)
    {
      if (this->m_ROI.height*this->m_ROI.width != ROI.height*ROI.width)
      {	
        delete[] intImage;
        delete[] intSqImage;
        intImage = new __uint32[(ROI.width+1)*(ROI.height+1)];
        intSqImage = new __uint64[(ROI.width+1)*(ROI.height+1)];
      }
      this->m_ROI = ROI;
      m_offset = ROI;
      return;
    }

    void ImageRepresentation::setNewImageSize( Rect ROI )
    {
      this->m_imageSize = ROI;
    }


    void ImageRepresentation::setNewImageAndROI(unsigned char* image, Rect ROI)
    {
      this->setNewROI(ROI);
      this->createIntegralsOfROI(image);
    }

    __uint32 ImageRepresentation::getValue(Point2D imagePosition)
    {
      Point2D position = imagePosition-m_offset;
      return intImage[position.row*(this->m_ROI.width+1)+position.col];
    }

    long ImageRepresentation::getSqSum(Rect imageROI)
    {
      // left upper Origin
      int OriginX = imageROI.left-m_offset.col;
      int OriginY = imageROI.upper-m_offset.row;

      __uint64 *OriginPtr = &intSqImage[OriginY * (m_ROI.width+1) + OriginX];

      // Check and fix width and height
      int Width  = imageROI.width;
      int Height = imageROI.height;

      if ( OriginX+Width  >= m_ROI.width  ) Width  = m_ROI.width  - OriginX;
      if ( OriginY+Height >= m_ROI.height ) Height = m_ROI.height  - OriginY;

      unsigned long down  = Height * (m_ROI.width+1);
      unsigned long right = Width;

      __int64 value = OriginPtr[down+right] + OriginPtr[0] - OriginPtr[right] - OriginPtr[down];

      assert (value >= 0);

      OriginPtr = NULL;
      return (long)value;

    }

    float ImageRepresentation::getVariance(Rect imageROI)
    {
      double area = imageROI.height*imageROI.width;
      double mean = (double) getSum(imageROI)/area;
      double sqSum = (double) getSqSum( imageROI );

      double variance = sqSum/area - (mean*mean);

      if( variance >= 0. )
        return (float)sqrt(variance);
      else
        return 1.0f;
    }

    __int32 ImageRepresentation::getSum(Rect imageROI)
    {
      // left upper Origin
      int OriginX = imageROI.left-m_offset.col;
      int OriginY = imageROI.upper-m_offset.row;

      __uint32 *OriginPtr = &intImage[OriginY * (m_ROI.width+1) + OriginX];

      // Check and fix width and height
      int Width  = imageROI.width;
      int Height = imageROI.height;

      if ( OriginX+Width  >= m_ROI.width  ) Width  = m_ROI.width  - OriginX;
      if ( OriginY+Height >= m_ROI.height ) Height = m_ROI.height  - OriginY;

      unsigned long down  = Height * (m_ROI.width+1);
      unsigned long right = Width;

      __int32 value = OriginPtr[down+right] + OriginPtr[0] - OriginPtr[right] - OriginPtr[down];


      OriginPtr = NULL;
      return value;
    }

    float ImageRepresentation::getMean(Rect imageROI)
    {
      // left upper Origin
      int OriginX = imageROI.left-m_offset.col;
      int OriginY = imageROI.upper-m_offset.row;

      // Check and fix width and height
      int Width  = imageROI.width;
      int Height = imageROI.height;

      if ( OriginX+Width  >= m_ROI.width  ) Width  = m_ROI.width  - OriginX;
      if ( OriginY+Height >= m_ROI.height ) Height = m_ROI.height  - OriginY;

      return getSum(imageROI)/static_cast<float>(Width*Height);
    }

    void ImageRepresentation::createIntegralsOfROI(unsigned char* image)
    {
      unsigned long ROIlength = (m_ROI.width+1)*(m_ROI.height+1);
      int columnidx, rowidx;
      unsigned long curPointer;
      unsigned long dptr;


      curPointer = 0;
      dptr = 0;

      memset(intImage, 0x00, ROIlength * sizeof( unsigned int ) );
      memset(intSqImage, 0x00, ROIlength * sizeof( unsigned __int64 ) ); 

      // current sum
      unsigned int value_tmp = 0;
      unsigned int value_tmpSq = 0;

      for (rowidx = 0; rowidx<m_ROI.height; rowidx++)
      {
        // current Image Position
        curPointer = (rowidx+m_ROI.upper)*m_imageSize.width+m_ROI.left;

        // current Integral Image Position
        dptr = (m_ROI.width+1)*(rowidx+1) + 1;

        value_tmp = 0;
        value_tmpSq = 0;

        for (columnidx = 0; columnidx<m_ROI.width; columnidx++)
        {
          // cumulative row sum
          value_tmp += image[curPointer];
          value_tmpSq += image[curPointer]*image[curPointer];

          // update Integral Image
          intImage[dptr] = intImage[ dptr - (m_ROI.width+1) ] + value_tmp;
          intSqImage[dptr] = intSqImage[ dptr - (m_ROI.width+1) ] + value_tmpSq;

          dptr++;
          curPointer++;
        }

      }

      return;
    }

    Patches::Patches(void)
    {
      this->num = 1;
      patches = new Rect;
      ROI.height = 0;
      ROI.width = 0;
      ROI.upper = 0;
      ROI.left = 0;
    }

    Patches::Patches(int num)
    {
      this->num = num;
      patches = new Rect[num];
      ROI.height = 0;
      ROI.width = 0;
      ROI.upper = 0;
      ROI.left = 0;
    }

    Patches::~Patches(void)
    {
      delete[] patches;
    }

    Rect Patches::getRect(int index)
    {
      if (index >= num)
        return Rect(-1, -1, -1, -1);
      if (index < 0) 
        return Rect(-1, -1, -1, -1);

      return patches[index];
    }

    int Patches::checkOverlap(Rect rect)
    {
      //loop over all patches and return the first found overap
      for (int curPatch = 0; curPatch< num; curPatch++)
      {
        Rect curRect = getRect (curPatch);
        int overlap = curRect.checkOverlap(rect);
        if (overlap > 0)
          return overlap;
      }
      return 0;
    }


    bool Patches::isDetection (Rect eval, unsigned char *labeledImg, int imgWidth)
    {
      bool isDet = false;
      Rect curRect;

      for (int curPatch = 0; curPatch < num; curPatch++)
      {
        curRect = getRect (curPatch);
        isDet = curRect.isDetection(eval, labeledImg, imgWidth);

        if (isDet)
        {
          break;
        }
      }

      return isDet;
    }

    Rect Patches::getSpecialRect (const char* what)
    {
      Rect r;
      r.height = -1;
      r.width = -1;
      r.upper = -1;
      r.left = -1;
      return r;
    }

    Rect Patches::getSpecialRect (const char* what, Size patchSize)
    {
      Rect r;
      r.height = -1;
      r.width = -1;
      r.upper = -1;
      r.left = -1;
      return r;
    }

    Rect Patches::getROI()
    {
      return ROI;
    }

    void Patches::setCheckedROI(Rect imageROI, Rect validROI)
    {
      int dCol, dRow;
      dCol = imageROI.left - validROI.left;
      dRow = imageROI.upper - validROI.upper;
      ROI.upper = (dRow < 0) ? validROI.upper : imageROI.upper;
      ROI.left = (dCol < 0) ? validROI.left : imageROI.left;
      dCol = imageROI.left+imageROI.width - (validROI.left+validROI.width);
      dRow = imageROI.upper+imageROI.height - (validROI.upper+validROI.height);
      ROI.height = (dRow > 0) ? validROI.height+validROI.upper-ROI.upper : imageROI.height+imageROI.upper-ROI.upper; 
      ROI.width = (dCol > 0) ? validROI.width+validROI.left-ROI.left : imageROI.width+imageROI.left-ROI.left; 
    }


    //-----------------------------------------------------------------------------
    PatchesRegularScan::PatchesRegularScan(Rect imageROI, Size patchSize, float relOverlap)
    {
      calculatePatches (imageROI, imageROI, patchSize, relOverlap);
    }

    PatchesRegularScan::PatchesRegularScan (Rect imageROI, Rect validROI, Size patchSize, float relOverlap)
    {
      calculatePatches (imageROI, validROI, patchSize, relOverlap);
    }

    void PatchesRegularScan::calculatePatches(Rect imageROI, Rect validROI, Size patchSize, float relOverlap)
    {
      if ((validROI == imageROI))
        ROI = imageROI;
      else
        setCheckedROI(imageROI, validROI);

      int stepCol = (int)floor((1.0f-relOverlap) * (float)patchSize.width+0.5f);
      int stepRow = (int)floor((1.0f-relOverlap) * (float)patchSize.height+0.5f);
      if (stepCol <= 0) stepCol = 1;
      if (stepRow <= 0) stepRow = 1;

      m_patchGrid.height = ((int)((float)(ROI.height-patchSize.height)/stepRow)+1);
      m_patchGrid.width = ((int)((float)(ROI.width-patchSize.width)/stepCol)+1);

      num = m_patchGrid.width * m_patchGrid.height;
      patches = new Rect[num];
      int curPatch = 0;

      m_rectUpperLeft = m_rectUpperRight = m_rectLowerLeft = m_rectLowerRight = patchSize;
      m_rectUpperLeft.upper = ROI.upper;
      m_rectUpperLeft.left = ROI.left;
      m_rectUpperRight.upper = ROI.upper;
      m_rectUpperRight.left = ROI.left+ROI.width-patchSize.width;
      m_rectLowerLeft.upper = ROI.upper+ROI.height-patchSize.height;
      m_rectLowerLeft.left = ROI.left;
      m_rectLowerRight.upper = ROI.upper+ROI.height-patchSize.height;
      m_rectLowerRight.left = ROI.left+ROI.width-patchSize.width;


      numPatchesX=0; numPatchesY=0;
      for (int curRow=0; curRow< ROI.height-patchSize.height+1; curRow+=stepRow)
      {
        numPatchesY++;

        for (int curCol=0; curCol< ROI.width-patchSize.width+1; curCol+=stepCol)
        {
          if(curRow == 0)
            numPatchesX++;

          patches[curPatch].width = patchSize.width;
          patches[curPatch].height = patchSize.height;
          patches[curPatch].upper = curRow+ROI.upper;
          patches[curPatch].left = curCol+ROI.left;
          curPatch++;
        }
      }

      assert (curPatch==num);
    }

    PatchesRegularScan::~PatchesRegularScan(void)
    {
    }

    Rect PatchesRegularScan::getSpecialRect(const char* what, Size patchSize)
    {
      Rect r;
      r.height = -1;
      r.width = -1;
      r.upper = -1;
      r.left = -1;
      return r;
    }

    Rect PatchesRegularScan::getSpecialRect(const char* what)
    {
      if (strcmp(what, "UpperLeft")==0) return m_rectUpperLeft;
      if (strcmp(what, "UpperRight")==0) return m_rectUpperRight;
      if (strcmp(what, "LowerLeft")==0) return m_rectLowerLeft;
      if (strcmp(what, "LowerRight")==0) return m_rectLowerRight;
      if (strcmp(what, "Random")==0)
      {
        int index = (rand()%(num));
        return patches[index];
      }

      // assert (false);
      return Rect(-1, -1, -1, -1); // fixed
    }

    //-----------------------------------------------------------------------------
    PatchesRegularScaleScan::PatchesRegularScaleScan (Rect imageROI, Size patchSize, float relOverlap, float scaleStart, float scaleEnd, float scaleFactor)
    {
      calculatePatches (imageROI, imageROI, patchSize, relOverlap, scaleStart, scaleEnd, scaleFactor);
    }

    PatchesRegularScaleScan::PatchesRegularScaleScan (Rect imageROI, Rect validROI, Size patchSize, float relOverlap, float scaleStart, float scaleEnd, float scaleFactor)
    {
      calculatePatches (imageROI, validROI, patchSize, relOverlap, scaleStart, scaleEnd, scaleFactor);
    }

    PatchesRegularScaleScan::~PatchesRegularScaleScan(void)
    {

    }


    void PatchesRegularScaleScan::calculatePatches(Rect imageROI, Rect validROI, Size patchSize, float relOverlap, float scaleStart, float scaleEnd, float scaleFactor)
    {

      if ((validROI == imageROI))
        ROI = imageROI;
      else
        setCheckedROI(imageROI, validROI);

      int numScales = (int)(log(scaleEnd/scaleStart)/log(scaleFactor));
      if (numScales < 0) numScales = 0;
      float curScaleFactor = 1;
      Size curPatchSize;
      int stepCol, stepRow;

      num = 0;
      for (int curScale = 0; curScale <= numScales; curScale++)
      {   
        curPatchSize = patchSize * (scaleStart*curScaleFactor);
        if (curPatchSize.height > ROI.height || curPatchSize.width > ROI.width)
        {
          numScales = curScale-1;
          break;
        }
        curScaleFactor *= scaleFactor;

        stepCol = (int)floor((1.0f-relOverlap) * (float)curPatchSize.width+0.5f);
        stepRow = (int)floor((1.0f-relOverlap) * (float)curPatchSize.height+0.5f);
        if (stepCol <= 0) stepCol = 1;
        if (stepRow <= 0) stepRow = 1;

        num += ((int)((float)(ROI.width-curPatchSize.width)/stepCol)+1)*((int)((float)(ROI.height-curPatchSize.height)/stepRow)+1);
      }
      patches = new Rect[num];

      int curPatch = 0;
      curScaleFactor = 1;
      for (int curScale = 0; curScale <= numScales; curScale++)
      {   
        curPatchSize = patchSize * (scaleStart*curScaleFactor);
        curScaleFactor *= scaleFactor;

        stepCol = (int)floor((1.0f-relOverlap) * (float)curPatchSize.width+0.5f);
        stepRow = (int)floor((1.0f-relOverlap) * (float)curPatchSize.height+0.5f);
        if (stepCol <= 0) stepCol = 1;
        if (stepRow <= 0) stepRow = 1;




        for (int curRow=0; curRow< ROI.height-curPatchSize.height+1; curRow+=stepRow)
        {
          for (int curCol=0; curCol<ROI.width-curPatchSize.width+1; curCol+=stepCol)
          {
            patches[curPatch].width = curPatchSize.width;
            patches[curPatch].height = curPatchSize.height;
            patches[curPatch].upper = curRow+ROI.upper;
            patches[curPatch].left = curCol+ROI.left;

            curPatch++;
          }
        }
      }
      assert (curPatch==num);

    }

    Rect PatchesRegularScaleScan::getSpecialRect (const char* what)
    {

      if (strcmp(what, "Random")==0)
      {
        int index = (rand()%(num));
        return patches[index];
      }

      Rect r;
      r.height = -1;
      r.width = -1;
      r.upper = -1;
      r.left = -1;
      return r;
    }
    Rect PatchesRegularScaleScan::getSpecialRect (const char* what, Size patchSize)
    {		
      if (strcmp(what, "UpperLeft")==0)
      {
        Rect rectUpperLeft;
        rectUpperLeft =  patchSize;
        rectUpperLeft.upper = ROI.upper;
        rectUpperLeft.left = ROI.left;
        return rectUpperLeft;
      }
      if (strcmp(what, "UpperRight")==0) 
      {
        Rect rectUpperRight;
        rectUpperRight = patchSize;
        rectUpperRight.upper = ROI.upper;
        rectUpperRight.left = ROI.left+ROI.width-patchSize.width;
        return rectUpperRight;
      }
      if (strcmp(what, "LowerLeft")==0)
      {
        Rect rectLowerLeft;
        rectLowerLeft = patchSize;
        rectLowerLeft.upper = ROI.upper+ROI.height-patchSize.height;
        rectLowerLeft.left = ROI.left;
        return rectLowerLeft;
      }
      if (strcmp(what, "LowerRight")==0)
      {
        Rect rectLowerRight;
        rectLowerRight = patchSize;
        rectLowerRight.upper = ROI.upper+ROI.height-patchSize.height;
        rectLowerRight.left = ROI.left+ROI.width-patchSize.width;
        return rectLowerRight;
      }
      if (strcmp(what, "Random")==0)
      {
        int index = (rand()%(num));
        return patches[index];
      }

      return Rect(-1, -1, -1, -1); 
    }

    EstimatedGaussDistribution::EstimatedGaussDistribution()
    {
      m_mean = 0;
      m_sigma = 1;
      this->m_P_mean = 1000;
      this->m_R_mean = 0.01f;
      this->m_P_sigma = 1000;
      this->m_R_sigma = 0.01f;
    }

    EstimatedGaussDistribution::EstimatedGaussDistribution(float P_mean, float R_mean, float P_sigma, float R_sigma)
    {
      m_mean = 0;
      m_sigma = 1;
      this->m_P_mean = P_mean;
      this->m_R_mean = R_mean;
      this->m_P_sigma = P_sigma;
      this->m_R_sigma = R_sigma;
    }


    EstimatedGaussDistribution::~EstimatedGaussDistribution()
    {
    }

    void EstimatedGaussDistribution::update(float value) 
    {
      //update distribution (mean and sigma) using a kalman filter for each

      float K;
      float minFactor = 0.001f; 

      //mean

      K = m_P_mean/(m_P_mean+m_R_mean);
      if (K < minFactor) K = minFactor;

      m_mean = K*value + (1.0f-K)*m_mean;
      m_P_mean = m_P_mean*m_R_mean/(m_P_mean+m_R_mean);


      K = m_P_sigma/(m_P_sigma+m_R_sigma);
      if (K < minFactor) K = minFactor;

      float tmp_sigma = K*(m_mean-value)*(m_mean-value) + (1.0f-K)*m_sigma*m_sigma;
      m_P_sigma = m_P_sigma*m_R_mean/(m_P_sigma+m_R_sigma);

      m_sigma = static_cast<float>( sqrt(tmp_sigma) );
      if (m_sigma <= 1.0f) m_sigma = 1.0f;

    }

    void EstimatedGaussDistribution::setValues(float mean, float sigma)
    {
      this->m_mean = mean;
      this->m_sigma = sigma;
    }

#define SQROOTHALF 0.7071
#define INITSIGMA( numAreas ) ( static_cast<float>( sqrt( 256.0f*256.0f / 12.0f * (numAreas) ) ) );

    FeatureHaar::FeatureHaar(Size patchSize)
      : m_areas(NULL), m_weights(NULL), m_scaleAreas(NULL), m_scaleWeights(NULL)
    {
      try {
        generateRandomFeature(patchSize);
      }
      catch (...) {
        delete[] m_scaleWeights;
        delete[] m_scaleAreas;
        delete[] m_areas;
        delete[] m_weights;
        throw;
      }
    }


    FeatureHaar::~FeatureHaar()
    {
      delete[] m_scaleWeights;
      delete[] m_scaleAreas;
      delete[] m_areas;
      delete[] m_weights;
    }

    void FeatureHaar::generateRandomFeature(Size patchSize)
    {	
      Point2D position;
      Size baseDim;
      Size sizeFactor;
      int area;

      Size minSize = Size(3,3);
      int minArea = 9;

      bool valid = false;
      while (!valid)
      {
        //chosse position and scale
        position.row = rand()%(patchSize.height);
        position.col = rand()%(patchSize.width);

        baseDim.width = (int) ((1-sqrt(1-(float)rand()/RAND_MAX))*patchSize.width);
        baseDim.height = (int) ((1-sqrt(1-(float)rand()/RAND_MAX))*patchSize.height);

        //select types
        //float probType[11] = {0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0909f, 0.0950f};
        float probType[11] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float prob = (float)rand()/RAND_MAX;

        if (prob < probType[0]) 
        {
          //check if feature is valid
          sizeFactor.height = 2;
          sizeFactor.width = 1;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type1");
          m_numAreas = 2;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 

          valid = true;

        }
        else if (prob < probType[0]+probType[1]) 
        {
          //check if feature is valid
          sizeFactor.height = 1;
          sizeFactor.width = 2;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type2");
          m_numAreas = 2;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;

        }
        else if (prob < probType[0]+probType[1]+probType[2]) 
        {
          //check if feature is valid
          sizeFactor.height = 4;
          sizeFactor.width = 1;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type3");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -2;
          m_weights[2] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = 2*baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row+3*baseDim.height;
          m_areas[2].left = position.col;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3])
        {
          //check if feature is valid
          sizeFactor.height = 1;
          sizeFactor.width = 4;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type3");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -2;
          m_weights[2] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = 2*baseDim.width;
          m_areas[2].upper = position.row;
          m_areas[2].left = position.col+3*baseDim.width;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3]+probType[4])
        {
          //check if feature is valid
          sizeFactor.height = 2;
          sizeFactor.width = 2;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type5");
          m_numAreas = 4;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -1;
          m_weights[2] = -1;
          m_weights[3] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row+baseDim.height;
          m_areas[2].left = position.col;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_areas[3].upper = position.row+baseDim.height;
          m_areas[3].left = position.col+baseDim.width;
          m_areas[3].height = baseDim.height;
          m_areas[3].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5])
        {
          //check if feature is valid
          sizeFactor.height = 3;
          sizeFactor.width = 3;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type6");
          m_numAreas = 2;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -9;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = 3*baseDim.height;
          m_areas[0].width = 3*baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_initMean = -8*128;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob< probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5]+probType[6]) 
        {
          //check if feature is valid
          sizeFactor.height = 3;
          sizeFactor.width = 1;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type7");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -2;
          m_weights[2] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row+baseDim.height*2;
          m_areas[2].left = position.col;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5]+probType[6]+probType[7])
        {
          //check if feature is valid
          sizeFactor.height = 1;
          sizeFactor.width = 3;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;

          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;

          if (area < minArea)
            continue;

          strcpy (m_type, "Type8");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -2;
          m_weights[2] = 1;
          m_areas= new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row;
          m_areas[2].left = position.col+2*baseDim.width;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5]+probType[6]+probType[7]+probType[8])
        {
          //check if feature is valid
          sizeFactor.height = 3;
          sizeFactor.width = 3;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type9");
          m_numAreas = 2;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -2;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = 3*baseDim.height;
          m_areas[0].width = 3*baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_initMean = 0;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob< probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5]+probType[6]+probType[7]+probType[8]+probType[9]) 
        {
          //check if feature is valid
          sizeFactor.height = 3;
          sizeFactor.width = 1;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type10");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -1;
          m_weights[2] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col;
          m_areas[1].upper = position.row+baseDim.height;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row+baseDim.height*2;
          m_areas[2].left = position.col;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 128;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else if (prob < probType[0]+probType[1]+probType[2]+probType[3]+probType[4]+probType[5]+probType[6]+probType[7]+probType[8]+probType[9]+probType[10])
        {
          //check if feature is valid
          sizeFactor.height = 1;
          sizeFactor.width = 3;
          if (position.row + baseDim.height*sizeFactor.height >= patchSize.height ||
            position.col + baseDim.width*sizeFactor.width >= patchSize.width)
            continue;
          area = baseDim.height*sizeFactor.height*baseDim.width*sizeFactor.width;
          if (area < minArea)
            continue;

          strcpy (m_type, "Type11");
          m_numAreas = 3;
          m_weights = new int[m_numAreas];
          m_weights[0] = 1;
          m_weights[1] = -1;
          m_weights[2] = 1;
          m_areas = new Rect[m_numAreas];
          m_areas[0].left = position.col;
          m_areas[0].upper = position.row;
          m_areas[0].height = baseDim.height;
          m_areas[0].width = baseDim.width;
          m_areas[1].left = position.col+baseDim.width;
          m_areas[1].upper = position.row;
          m_areas[1].height = baseDim.height;
          m_areas[1].width = baseDim.width;
          m_areas[2].upper = position.row;
          m_areas[2].left = position.col+2*baseDim.width;
          m_areas[2].height = baseDim.height;
          m_areas[2].width = baseDim.width;
          m_initMean = 128;
          m_initSigma = INITSIGMA( m_numAreas ); 
          valid = true;
        }
        else
          assert (false);	
      }

      m_initSize = patchSize;
      m_curSize = m_initSize;
      m_scaleFactorWidth = m_scaleFactorHeight = 1.0f;
      m_scaleAreas = new Rect[m_numAreas];
      m_scaleWeights = new float[m_numAreas];
      for (int curArea = 0; curArea<m_numAreas; curArea++) {
        m_scaleAreas[curArea] = m_areas[curArea];
        m_scaleWeights[curArea] = (float)m_weights[curArea] /
          (float)(m_areas[curArea].width*m_areas[curArea].height);
      }
    }

    bool FeatureHaar::eval(ImageRepresentation* image, Rect ROI, float* result) 
    {
      *result = 0.0f;
      Point2D offset;
      offset = ROI;

      // define the minimum size
      Size minSize = Size(3,3);

      // printf("in eval %d = %d\n",curSize.width,ROI.width );

      if ( m_curSize.width != ROI.width || m_curSize.height != ROI.height )
      {
        m_curSize = ROI;
        if (!(m_initSize==m_curSize))
        {
          m_scaleFactorHeight = (float)m_curSize.height/m_initSize.height;
          m_scaleFactorWidth = (float)m_curSize.width/m_initSize.width;

          for (int curArea = 0; curArea < m_numAreas; curArea++)
          {
            m_scaleAreas[curArea].height = (int)floor((float)m_areas[curArea].height*m_scaleFactorHeight+0.5f);
            m_scaleAreas[curArea].width = (int)floor((float)m_areas[curArea].width*m_scaleFactorWidth+0.5f);

            if (m_scaleAreas[curArea].height < minSize.height || m_scaleAreas[curArea].width < minSize.width) {
              m_scaleFactorWidth = 0.0f;
              return false;
            }

            m_scaleAreas[curArea].left = (int)floor( (float)m_areas[curArea].left*m_scaleFactorWidth+0.5f);
            m_scaleAreas[curArea].upper = (int)floor( (float)m_areas[curArea].upper*m_scaleFactorHeight+0.5f);
            m_scaleWeights[curArea] = (float)m_weights[curArea] /
              (float)((m_scaleAreas[curArea].width)*(m_scaleAreas[curArea].height));  
          }
        }
        else
        {
          m_scaleFactorWidth = m_scaleFactorHeight = 1.0f;
          for (int curArea = 0; curArea<m_numAreas; curArea++) {
            m_scaleAreas[curArea] = m_areas[curArea];
            m_scaleWeights[curArea] = (float)m_weights[curArea] /
              (float)((m_areas[curArea].width)*(m_areas[curArea].height));
          }
        }
      }

      if ( m_scaleFactorWidth == 0.0f )
        return false;

      for (int curArea = 0; curArea < m_numAreas; curArea++)
      {
        *result += (float)image->getSum( m_scaleAreas[curArea]+offset )*
          m_scaleWeights[curArea];
      }

      if (image->getUseVariance())
      {
        float variance = (float) image->getVariance(ROI);
        *result /=  variance;
      }

      m_response = *result;

      return true;
    }

    void FeatureHaar::getInitialDistribution(EstimatedGaussDistribution* distribution)
    {
      distribution->setValues(m_initMean, m_initSigma);
    }

    ClassifierThreshold::ClassifierThreshold()
    {
      m_posSamples = new EstimatedGaussDistribution();
      m_negSamples = new EstimatedGaussDistribution();
      m_threshold = 0.0f;
      m_parity = 0;
    }

    ClassifierThreshold::~ClassifierThreshold()
    {
      if (m_posSamples!=NULL) delete m_posSamples;
      if (m_negSamples!=NULL) delete m_negSamples;
    }

    void* ClassifierThreshold::getDistribution (int target)
    {
      if (target == 1)
        return m_posSamples;
      else
        return m_negSamples;
    }

    void ClassifierThreshold::update(float value, int target)
    {
      //update distribution
      if (target == 1)
        m_posSamples->update(value); 
      else
        m_negSamples->update(value); 

      //adapt threshold and parity
      m_threshold = (m_posSamples->getMean()+m_negSamples->getMean())/2.0f;
      m_parity = (m_posSamples->getMean() > m_negSamples->getMean()) ? 1 : -1;
    }

    int ClassifierThreshold::eval(float value)
    {
      return (((m_parity*(value-m_threshold))>0) ? 1 : -1);
    }

    WeakClassifier::WeakClassifier()
    {
    }

    WeakClassifier::~WeakClassifier()
    {
    }


    bool WeakClassifier::update(ImageRepresentation* image, Rect ROI, int target) 
    {
      return true;
    }

    int WeakClassifier::eval(ImageRepresentation* image, Rect  ROI) 
    {
      return 0;
    }

    int WeakClassifier::getType ()
    {
      return 0;
    }

    float WeakClassifier::getValue (ImageRepresentation* image, Rect  ROI)
    {
      return 0;
    }

    WeakClassifierHaarFeature::WeakClassifierHaarFeature(Size patchSize)
    {
      m_feature = new FeatureHaar(patchSize);
      generateRandomClassifier();
      m_feature->getInitialDistribution((EstimatedGaussDistribution*) m_classifier->getDistribution(-1));
      m_feature->getInitialDistribution((EstimatedGaussDistribution*) m_classifier->getDistribution(1));
    }

    void WeakClassifierHaarFeature::resetPosDist()
    {
      m_feature->getInitialDistribution((EstimatedGaussDistribution*) m_classifier->getDistribution(1));
      m_feature->getInitialDistribution((EstimatedGaussDistribution*) m_classifier->getDistribution(-1));
    }

    WeakClassifierHaarFeature::~WeakClassifierHaarFeature()
    {
      delete m_classifier;
      delete m_feature;

    }

    void WeakClassifierHaarFeature::generateRandomClassifier()
    {
      m_classifier = new ClassifierThreshold();
    }

    bool WeakClassifierHaarFeature::update(ImageRepresentation *image, Rect ROI, int target) 
    {
      float value;

      bool valid = m_feature->eval (image, ROI, &value); 
      if (!valid)
        return true;

      m_classifier->update(value, target);
      return (m_classifier->eval(value) != target);
    }

    int WeakClassifierHaarFeature::eval(ImageRepresentation *image, Rect ROI) 
    {
      float value;
      bool valid = m_feature->eval(image, ROI, &value); 
      if (!valid)
        return 0;

      return m_classifier->eval(value);
    }

    float WeakClassifierHaarFeature::getValue(ImageRepresentation *image, Rect ROI)
    {
      float value;
      bool valid = m_feature->eval (image, ROI, &value);
      if (!valid)
        return 0;

      return value;
    }

    EstimatedGaussDistribution* WeakClassifierHaarFeature::getPosDistribution()
    {
      return static_cast<EstimatedGaussDistribution*>(m_classifier->getDistribution(1));
    }


    EstimatedGaussDistribution* WeakClassifierHaarFeature::getNegDistribution()
    {
      return static_cast<EstimatedGaussDistribution*>(m_classifier->getDistribution(-1));
    }

    BaseClassifier::BaseClassifier(int numWeakClassifier, int iterationInit, Size patchSize)
    {
      this->m_numWeakClassifier = numWeakClassifier;
      this->m_iterationInit = iterationInit;

      weakClassifier = new WeakClassifier*[numWeakClassifier+iterationInit];
      m_idxOfNewWeakClassifier = numWeakClassifier;

      generateRandomClassifier(patchSize);

      m_referenceWeakClassifier = false;
      m_selectedClassifier = 0;

      m_wCorrect = new float[numWeakClassifier+iterationInit];
      memset (m_wCorrect, 0, sizeof(float)*numWeakClassifier+iterationInit);

      m_wWrong = new float[numWeakClassifier+iterationInit];
      memset (m_wWrong, 0, sizeof(float)*numWeakClassifier+iterationInit);

      for (int curWeakClassifier = 0; curWeakClassifier < numWeakClassifier+iterationInit; curWeakClassifier++)
        m_wWrong[curWeakClassifier] = m_wCorrect[curWeakClassifier] = 1;
    }

    BaseClassifier::BaseClassifier(int numWeakClassifier, int iterationInit, WeakClassifier** weakClassifier)
    {
      this->m_numWeakClassifier = numWeakClassifier;
      this->m_iterationInit = iterationInit;
      this->weakClassifier = weakClassifier;
      m_referenceWeakClassifier = true;
      m_selectedClassifier = 0;
      m_idxOfNewWeakClassifier = numWeakClassifier;

      m_wCorrect = new float[numWeakClassifier+iterationInit];
      memset (m_wCorrect, 0, sizeof(float)*numWeakClassifier+iterationInit);
      m_wWrong = new float[numWeakClassifier+iterationInit];
      memset (m_wWrong, 0, sizeof(float)*numWeakClassifier+iterationInit);

      for (int curWeakClassifier = 0; curWeakClassifier < numWeakClassifier+iterationInit; curWeakClassifier++)
        m_wWrong[curWeakClassifier] = m_wCorrect[curWeakClassifier] = 1;
    }

    BaseClassifier::~BaseClassifier()
    {
      if (!m_referenceWeakClassifier)
      {
        for (int curWeakClassifier = 0; curWeakClassifier< m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
          delete weakClassifier[curWeakClassifier];

        delete[] weakClassifier;
      }
      if (m_wCorrect!=NULL) delete[] m_wCorrect;
      if (m_wWrong!=NULL) delete[] m_wWrong;

    }

    void BaseClassifier::generateRandomClassifier (Size patchSize)
    {
      for (int curWeakClassifier = 0; curWeakClassifier< m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
      {
        weakClassifier[curWeakClassifier] = new WeakClassifierHaarFeature(patchSize);
      }
    }

    int BaseClassifier::eval(ImageRepresentation *image, Rect ROI) 
    {
      return weakClassifier[m_selectedClassifier]->eval(image, ROI);
    }

    float BaseClassifier::getValue (ImageRepresentation *image, Rect ROI, int weakClassifierIdx)
    {
      if (weakClassifierIdx < 0 || weakClassifierIdx >= m_numWeakClassifier ) 
        return weakClassifier[m_selectedClassifier]->getValue(image, ROI);
      return weakClassifier[weakClassifierIdx]->getValue(image, ROI);
    }

    void BaseClassifier::trainClassifier(ImageRepresentation* image, Rect ROI, int target, float importance, bool* errorMask) 
    {
      //get poisson value
      double A = 1;
      int K = 0;
      int K_max = 10;
      while (1)
      {
        double U_k = (double)rand()/RAND_MAX;
        A*=U_k;
        if (K > K_max || A<exp(-importance))
          break;
        K++;
      }

      for (int curK = 0; curK <= K; curK++)
        for (int curWeakClassifier = 0; curWeakClassifier < m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
          errorMask[curWeakClassifier] = weakClassifier[curWeakClassifier]->update (image, ROI, target); 

    }

    void BaseClassifier::getErrorMask(ImageRepresentation* image, Rect ROI, int target, bool* errorMask) 
    {
      for (int curWeakClassifier = 0; curWeakClassifier < m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
        errorMask[curWeakClassifier] = (weakClassifier[curWeakClassifier]->eval(image, ROI) != target);
    }

    float BaseClassifier::getError(int curWeakClassifier)
    {
      if (curWeakClassifier == -1)
        curWeakClassifier = m_selectedClassifier;
      return m_wWrong[curWeakClassifier]/(m_wWrong[curWeakClassifier]+m_wCorrect[curWeakClassifier]);
    }

    int BaseClassifier::selectBestClassifier(bool* errorMask, float importance, float* errors) 
    {
      float minError = FLOAT_MAX;
      int tmp_selectedClassifier = m_selectedClassifier;

      for (int curWeakClassifier = 0; curWeakClassifier < m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
      {
        if (errorMask[curWeakClassifier])
        {
          m_wWrong[curWeakClassifier] +=  importance;
        }
        else
        {
          m_wCorrect[curWeakClassifier] += importance;
        }

        if (errors[curWeakClassifier]==FLOAT_MAX)
          continue;

        errors[curWeakClassifier] = m_wWrong[curWeakClassifier]/
          (m_wWrong[curWeakClassifier]+m_wCorrect[curWeakClassifier]);

        /*if(errors[curWeakClassifier] < 0.001 || !(errors[curWeakClassifier]>0.0))
        {
        errors[curWeakClassifier] = 0.001;
        }

        if(errors[curWeakClassifier] >= 1.0)
        errors[curWeakClassifier] = 0.999;

        assert (errors[curWeakClassifier] > 0.0);
        assert (errors[curWeakClassifier] < 1.0);*/

        if (curWeakClassifier < m_numWeakClassifier)
        {		
          if (errors[curWeakClassifier] < minError)
          {
            minError = errors[curWeakClassifier];
            tmp_selectedClassifier = curWeakClassifier;
          }
        }
      }

      m_selectedClassifier = tmp_selectedClassifier;
      return m_selectedClassifier;
    }

    void BaseClassifier::getErrors(float* errors)
    {
      for (int curWeakClassifier = 0; curWeakClassifier < m_numWeakClassifier+m_iterationInit; curWeakClassifier++)
      {	
        if (errors[curWeakClassifier]==FLOAT_MAX)
          continue;

        errors[curWeakClassifier] = m_wWrong[curWeakClassifier]/
          (m_wWrong[curWeakClassifier]+m_wCorrect[curWeakClassifier]);

        assert (errors[curWeakClassifier] > 0);
      }
    }

    int BaseClassifier::replaceWeakestClassifier(float* errors, Size patchSize)
    {
      float maxError = 0.0f;
      int index = -1;

      //search the classifier with the largest error
      for (int curWeakClassifier = m_numWeakClassifier-1; curWeakClassifier >= 0; curWeakClassifier--)
      {
        if (errors[curWeakClassifier] > maxError)
        {
          maxError = errors[curWeakClassifier];
          index = curWeakClassifier;
        }
      }

      assert (index > -1);
      assert (index != m_selectedClassifier);

      //replace
      m_idxOfNewWeakClassifier++;
      if (m_idxOfNewWeakClassifier == m_numWeakClassifier+m_iterationInit) 
        m_idxOfNewWeakClassifier = m_numWeakClassifier;

      if (maxError > errors[m_idxOfNewWeakClassifier])
      {
        delete weakClassifier[index];
        weakClassifier[index] = weakClassifier[m_idxOfNewWeakClassifier];
        m_wWrong[index] = m_wWrong[m_idxOfNewWeakClassifier];
        m_wWrong[m_idxOfNewWeakClassifier] = 1;
        m_wCorrect[index] = m_wCorrect[m_idxOfNewWeakClassifier];
        m_wCorrect[m_idxOfNewWeakClassifier] = 1;

        weakClassifier[m_idxOfNewWeakClassifier] = new WeakClassifierHaarFeature (patchSize);

        return index;
      }
      else
        return -1;

    }

    void BaseClassifier::replaceClassifierStatistic(int sourceIndex, int targetIndex)
    {
      assert (targetIndex >=0);
      assert (targetIndex != m_selectedClassifier);
      assert (targetIndex < m_numWeakClassifier);

      //replace
      m_wWrong[targetIndex] = m_wWrong[sourceIndex];
      m_wWrong[sourceIndex] = 1.0f;
      m_wCorrect[targetIndex] = m_wCorrect[sourceIndex];
      m_wCorrect[sourceIndex] = 1.0f;
    }

    StrongClassifier::StrongClassifier(int numBaseClassifier, 
      int numWeakClassifier, 
      Size patchSize, 
      bool useFeatureExchange, 
      int iterationInit)
    {
      this->numBaseClassifier = numBaseClassifier;
      this->numAllWeakClassifier = numWeakClassifier+iterationInit;

      alpha = new float[numBaseClassifier];
      memset(alpha, 0x00, sizeof(float)*numBaseClassifier);

      this->patchSize = patchSize;
      this->useFeatureExchange = useFeatureExchange;
    }

    StrongClassifier::~StrongClassifier()
    {
      for (int curBaseClassifier = 0; curBaseClassifier< numBaseClassifier; curBaseClassifier++)
        delete baseClassifier[curBaseClassifier];
      delete[] baseClassifier;
      delete[] alpha;
    }

    float StrongClassifier::getFeatureValue(ImageRepresentation *image, Rect ROI, int baseClassifierIdx)
    {
      return baseClassifier[baseClassifierIdx]->getValue(image, ROI);
    }


    float StrongClassifier::eval(ImageRepresentation *image, Rect ROI)
    {
      float value = 0.0f;
      int curBaseClassifier=0;

      for (curBaseClassifier = 0; curBaseClassifier<numBaseClassifier; curBaseClassifier++)
        value+= baseClassifier[curBaseClassifier]->eval(image, ROI)*alpha[curBaseClassifier];

      return value;
    }

    bool StrongClassifier::update(ImageRepresentation *image, Rect ROI, int target, float importance) 
    {
      assert (true);
      return false;
    }

    bool StrongClassifier::updateSemi(ImageRepresentation *image, Rect ROI, float priorConfidence)
    {
      assert (true);
      return false;
    }


    float StrongClassifier::getSumAlpha (int toBaseClassifier)
    {
      float sumAlpha = 0;
      if (toBaseClassifier == -1)
        toBaseClassifier = numBaseClassifier;

      for (int curBaseClassifier=0; curBaseClassifier < toBaseClassifier; curBaseClassifier++)
        sumAlpha+= alpha[curBaseClassifier];

      return sumAlpha;
    }

    float StrongClassifier::getImportance(ImageRepresentation *image, Rect ROI, int target, int numBaseClassifier)
    {
      if (numBaseClassifier == -1) 
        numBaseClassifier =  this->numBaseClassifier;

      float importance = 1.0f;

      for (int curBaseClassifier = 0; curBaseClassifier<numBaseClassifier; curBaseClassifier++)
      {
        bool error = (baseClassifier[curBaseClassifier]->eval (image, ROI)!= target);

        if(error)
          importance/=(2*baseClassifier[curBaseClassifier]->getError());
        else
          importance/=(2*(1-baseClassifier[curBaseClassifier]->getError()));
      }

      return importance/numBaseClassifier;
    }


    void StrongClassifier::resetWeightDistribution()
    {
      for (int curBaseClassifier = 0; curBaseClassifier<numBaseClassifier; curBaseClassifier++)
      {
        for(int curWeakClassifier = 0; curWeakClassifier < baseClassifier[curBaseClassifier]->getNumWeakClassifier(); curWeakClassifier++)
        {
          baseClassifier[curBaseClassifier]->setWCorrect(curWeakClassifier, 1.0);
          baseClassifier[curBaseClassifier]->setWWrong(curWeakClassifier, 1.0);
        }
      }
    }

    StrongClassifierDirectSelection::StrongClassifierDirectSelection(int numBaseClassifier, int numWeakClassifier, 
      Size patchSize, bool useFeatureExchange, int iterationInit) 
      : StrongClassifier(numBaseClassifier, numWeakClassifier, patchSize, useFeatureExchange, iterationInit)
    {
      this->useFeatureExchange = useFeatureExchange;
      baseClassifier = new BaseClassifier*[numBaseClassifier];
      baseClassifier[0] = new BaseClassifier(numWeakClassifier, iterationInit, patchSize);

      for (int curBaseClassifier = 1; curBaseClassifier< numBaseClassifier; curBaseClassifier++)
        baseClassifier[curBaseClassifier] = new BaseClassifier(numWeakClassifier, iterationInit, baseClassifier[0]->getReferenceWeakClassifier());

      m_errorMask = new bool[numAllWeakClassifier];
      m_errors = new float[numAllWeakClassifier];
      m_sumErrors = new float[numAllWeakClassifier];
    }

    StrongClassifierDirectSelection::~StrongClassifierDirectSelection()
    {
      delete[] m_errorMask;
      delete[] m_errors;
      delete[] m_sumErrors;
    }


    bool StrongClassifierDirectSelection::update(ImageRepresentation *image, Rect ROI, int target, float importance)
    {
      memset(m_errorMask, 0, numAllWeakClassifier*sizeof(bool));
      memset(m_errors, 0, numAllWeakClassifier*sizeof(float));
      memset(m_sumErrors, 0, numAllWeakClassifier*sizeof(float));

      baseClassifier[0]->trainClassifier(image, ROI, target, importance, m_errorMask);
      for (int curBaseClassifier = 0; curBaseClassifier < numBaseClassifier; curBaseClassifier++)
      {
        int selectedClassifier = baseClassifier[curBaseClassifier]->selectBestClassifier(m_errorMask, importance, m_errors);

        if (m_errors[selectedClassifier] >= 0.5)
          alpha[curBaseClassifier] = 0;
        else
          alpha[curBaseClassifier] = logf((1.0f-m_errors[selectedClassifier])/m_errors[selectedClassifier]);

        if(m_errorMask[selectedClassifier])
          importance *= (float)sqrt((1.0f-m_errors[selectedClassifier])/m_errors[selectedClassifier]);
        else
          importance *= (float)sqrt(m_errors[selectedClassifier]/(1.0f-m_errors[selectedClassifier]));

        //weight limitation
        //if (importance > 100) importance = 100;

        //sum up errors
        for (int curWeakClassifier = 0; curWeakClassifier<numAllWeakClassifier; curWeakClassifier++)
        {
          if (m_errors[curWeakClassifier]!=FLOAT_MAX && m_sumErrors[curWeakClassifier]>=0)
            m_sumErrors[curWeakClassifier]+= m_errors[curWeakClassifier];
        }

        //mark feature as used
        m_sumErrors[selectedClassifier] = -1;
        m_errors[selectedClassifier] = FLOAT_MAX;
      }

      if (useFeatureExchange)
      {
        int replacedClassifier = baseClassifier[0]->replaceWeakestClassifier (m_sumErrors, patchSize);
        if (replacedClassifier > 0)
          for (int curBaseClassifier = 1; curBaseClassifier < numBaseClassifier; curBaseClassifier++)
            baseClassifier[curBaseClassifier]->replaceClassifierStatistic(baseClassifier[0]->getIdxOfNewWeakClassifier(), replacedClassifier);
      }

      return true;
    }

    StrongClassifierStandard::StrongClassifierStandard(int numBaseClassifier, 
      int numWeakClassifier, 
      Size patchSize, 
      bool useFeatureExchange,
      int iterationInit) 
      : StrongClassifier(  numBaseClassifier, 
      numWeakClassifier, 
      patchSize,
      useFeatureExchange,
      iterationInit)
    {
      // init Base Classifier
      baseClassifier = new BaseClassifier*[numBaseClassifier];

      for (int curBaseClassifier = 0; curBaseClassifier< numBaseClassifier; curBaseClassifier++)
      {
        baseClassifier[curBaseClassifier] = new BaseClassifier(numWeakClassifier, iterationInit, patchSize);
      }

      m_errorMask = new bool[numAllWeakClassifier];
      m_errors = new float[numAllWeakClassifier];
    }

    StrongClassifierStandard::~StrongClassifierStandard()
    {
      delete[] m_errorMask;
      delete[] m_errors;
    }


    bool StrongClassifierStandard::update(ImageRepresentation *image, Rect ROI, int target, float importance)
    {
      int curBaseClassifier;
      for (curBaseClassifier = 0; curBaseClassifier<numBaseClassifier; curBaseClassifier++)
      {
        memset(m_errorMask, 0x00, numAllWeakClassifier*sizeof(bool));
        memset(m_errors, 0x00, numAllWeakClassifier*sizeof(float));

        int selectedClassifier;
        baseClassifier[curBaseClassifier]->trainClassifier(image, ROI, target, importance, m_errorMask);
        selectedClassifier = baseClassifier[curBaseClassifier]->selectBestClassifier (m_errorMask, importance, m_errors);

        if (m_errors[selectedClassifier] >= 0.5)
          alpha[curBaseClassifier] = 0;
        else
          alpha[curBaseClassifier] = logf((1.0f-m_errors[selectedClassifier])/m_errors[selectedClassifier]);

        if(m_errorMask[selectedClassifier])
          importance *= (float)sqrt((1.0f-m_errors[selectedClassifier])/m_errors[selectedClassifier]);
        else
          importance *= (float)sqrt(m_errors[selectedClassifier]/(1.0f-m_errors[selectedClassifier]));			

        //weight limitation
        //if (importance > 100) importance = 100;

        if (useFeatureExchange)
          baseClassifier[curBaseClassifier]->replaceWeakestClassifier (m_errors, patchSize);

      }

      return true;
    }

    StrongClassifierStandardSemi::StrongClassifierStandardSemi(int numBaseClassifier, 
      int numWeakClassifier, 
      Size patchSize, 
      bool useFeatureExchange,
      int iterationInit)
      : StrongClassifier(  numBaseClassifier, 
      numWeakClassifier, 
      patchSize,
      useFeatureExchange,
      iterationInit)
    {
      // init Base Classifier
      baseClassifier = new BaseClassifier*[numBaseClassifier];

      m_pseudoTarget = new int[numBaseClassifier];
      m_pseudoLambda = new float[numBaseClassifier];

      for (int curBaseClassifier = 0; curBaseClassifier< numBaseClassifier; curBaseClassifier++)
      {
        baseClassifier[curBaseClassifier] = new BaseClassifier(numWeakClassifier, iterationInit, patchSize);
      }

      m_errorMask = new bool[numAllWeakClassifier];
      m_errors = new float[numAllWeakClassifier];
    }

    StrongClassifierStandardSemi::~StrongClassifierStandardSemi()
    {
      delete[] m_errorMask;
      delete[] m_errors;

      delete[] m_pseudoTarget;
      delete[] m_pseudoLambda;
    }



    bool StrongClassifierStandardSemi::updateSemi(ImageRepresentation *image, Rect ROI, float priorConfidence)
    {

      float value = 0.0f, kvalue = 0.0f;

      bool decided = false;
      bool uncertain = false;
      bool used = false;
      float sumAlpha = 0;

      float scaleFactor = 2.0f;

      int curBaseClassifier;
      for (curBaseClassifier = 0; curBaseClassifier<numBaseClassifier; curBaseClassifier++)
      {
        memset(m_errorMask, 0x00, numAllWeakClassifier*sizeof(bool));
        memset(m_errors, 0x00, numAllWeakClassifier*sizeof(float));

        int selectedClassifier;
        {
          //scale
          if (sumAlpha > 0)
            kvalue = value/this->getSumAlpha();
          else
            kvalue = 0;

          float combinedDecision = tanh(scaleFactor*priorConfidence)-tanh(scaleFactor*kvalue);
          int myTarget = static_cast<int>(sign(combinedDecision));

          m_pseudoTarget[curBaseClassifier] = myTarget;
          float myImportance = ::abs(combinedDecision);
          m_pseudoLambda[curBaseClassifier] = myImportance;

          baseClassifier[curBaseClassifier]->trainClassifier(image, ROI, myTarget, myImportance, m_errorMask);
          selectedClassifier = baseClassifier[curBaseClassifier]->selectBestClassifier (m_errorMask, myImportance, m_errors);
        }

        float curValue = baseClassifier[curBaseClassifier]->eval(image, ROI)*alpha[curBaseClassifier];
        value += curValue;
        sumAlpha +=alpha[curBaseClassifier];

        if (m_errors[selectedClassifier] >= 0.5)
          alpha[curBaseClassifier] = 0;
        else
          alpha[curBaseClassifier] = logf((1.0f-m_errors[selectedClassifier])/m_errors[selectedClassifier]);

        if (useFeatureExchange)
          baseClassifier[curBaseClassifier]->replaceWeakestClassifier (m_errors, patchSize);

      }

      return used;
    }



    Detector::Detector(StrongClassifier* classifier)
    {
      this->m_classifier = classifier;

      m_confidences = NULL;
      m_sizeConfidences = 0;
      m_maxConfidence = -FLOAT_MAX;
      m_numDetections = 0;
      m_idxDetections = NULL;
      m_idxBestDetection = -1;

      m_confMatrix = cvCreateMat(1,1,CV_32FC1);
      m_confMatrixSmooth = cvCreateMat(1,1,CV_32FC1);
      m_confImageDisplay = cvCreateImage(cvSize(1,1),IPL_DEPTH_8U,1);

    }

    Detector::~Detector()
    {
      if (m_idxDetections != NULL)
        delete[] m_idxDetections;
      if (m_confidences != NULL)
        delete[] m_confidences;

      cvReleaseMat(&m_confMatrix);
      cvReleaseMat(&m_confMatrixSmooth);
      cvReleaseImage(&m_confImageDisplay);

    }

    void Detector::prepareConfidencesMemory(int numPatches)
    {
      if ( numPatches <= m_sizeConfidences )	
        return;							

      if ( m_confidences )
        delete[] m_confidences;

      m_sizeConfidences = numPatches;
      m_confidences = new float[numPatches];
    }

    void Detector::prepareDetectionsMemory(int numDetections)
    {
      if ( numDetections <= m_sizeDetections )	
        return;							

      if ( m_idxDetections )
        delete[] m_idxDetections;
      m_sizeDetections = numDetections;
      m_idxDetections = new int[numDetections];
    }


    void Detector::classify(ImageRepresentation* image, Patches* patches, float minMargin)
    {
      int numPatches = patches->getNum();

      prepareConfidencesMemory(numPatches);

      m_numDetections = 0;
      m_idxBestDetection = -1;
      m_maxConfidence = -FLOAT_MAX;
      int numBaseClassifiers = m_classifier->getNumBaseClassifier();

      for (int curPatch=0; curPatch < numPatches; curPatch++)
      {
        m_confidences[curPatch] = m_classifier->eval(image, patches->getRect(curPatch));

        if (m_confidences[curPatch] > m_maxConfidence)
        {
          m_maxConfidence = m_confidences[curPatch];
          m_idxBestDetection = curPatch;
        }
        if (m_confidences[curPatch] > minMargin)
          m_numDetections++;
      }

      prepareDetectionsMemory(m_numDetections);
      int curDetection = -1;
      for (int curPatch=0; curPatch < numPatches; curPatch++)
      {
        if (m_confidences[curPatch]>minMargin) m_idxDetections[++curDetection]=curPatch;
      }
    }


    void Detector::classifySmooth(ImageRepresentation* image, Patches* patches, float minMargin)
    {
      int numPatches = patches->getNum();

      prepareConfidencesMemory(numPatches);

      m_numDetections = 0;
      m_idxBestDetection = -1;
      m_maxConfidence = -FLOAT_MAX;
      int numBaseClassifiers = m_classifier->getNumBaseClassifier();

      PatchesRegularScan *regPatches = (PatchesRegularScan*)patches;
      Size patchGrid = regPatches->getPatchGrid();

      if((patchGrid.width != m_confMatrix->cols) || (patchGrid.height != m_confMatrix->rows)) {
        cvReleaseMat(&m_confMatrix);
        cvReleaseMat(&m_confMatrixSmooth);
        cvReleaseImage(&m_confImageDisplay);

        m_confMatrix = cvCreateMat(patchGrid.height,patchGrid.width,CV_32FC1);
        m_confMatrixSmooth = cvCreateMat(patchGrid.height,patchGrid.width,CV_32FC1);
        m_confImageDisplay = cvCreateImage(cvSize(patchGrid.width,patchGrid.height),IPL_DEPTH_8U,1);
      }

      int curPatch = 0;
      // Eval and filter
      for(int row = 0; row < patchGrid.height; row++) 
      {
        for( int col = 0; col < patchGrid.width; col++) 
        {
          //int returnedInLayer;
          m_confidences[curPatch] = m_classifier->eval(image, patches->getRect(curPatch)); 

          // fill matrix
          cvmSet(m_confMatrix,row,col,m_confidences[curPatch]);
          curPatch++;
        }
      }

      // Filter
      cvSmooth(m_confMatrix,m_confMatrixSmooth,CV_GAUSSIAN,3);

      // Make display friendly
      double min_val, max_val;
      cvMinMaxLoc(m_confMatrixSmooth, &min_val, &max_val);
      for (int y = 0; y < m_confImageDisplay->height; y++)
      {
        unsigned char* pConfImg = (unsigned char*)(m_confImageDisplay->imageData + y*m_confImageDisplay->widthStep);
        const float* pConfData = m_confMatrixSmooth->data.fl + y*m_confMatrixSmooth->step/sizeof(float);
        for (int x = 0; x < m_confImageDisplay->width; x++, pConfImg++, pConfData++)
        {
          *pConfImg = static_cast<unsigned char>( 255.0*(*pConfData-min_val) / (max_val-min_val) );
        }
      }

      // Get best detection
      curPatch = 0;
      for(int row = 0; row < patchGrid.height; row++) {
        for( int col = 0; col < patchGrid.width; col++) {
          // fill matrix
          m_confidences[curPatch] = (float)cvmGet(m_confMatrixSmooth,row,col);

          if (m_confidences[curPatch] > m_maxConfidence)
          {
            m_maxConfidence = m_confidences[curPatch];
            m_idxBestDetection = curPatch;
          }
          if (m_confidences[curPatch] > minMargin) {
            m_numDetections++;
          }
          curPatch++;
        }
      }

      prepareDetectionsMemory(m_numDetections);
      int curDetection = -1;
      for (int curPatch=0; curPatch < numPatches; curPatch++)
      {
        if (m_confidences[curPatch]>minMargin) m_idxDetections[++curDetection]=curPatch;
      }
    }



    int Detector::getNumDetections()
    {
      return m_numDetections; 
    }

    float Detector::getConfidence(int patchIdx)
    {
      return m_confidences[patchIdx];
    }

    float Detector::getConfidenceOfDetection (int detectionIdx)
    {
      return m_confidences[getPatchIdxOfDetection(detectionIdx)];
    }

    int Detector::getPatchIdxOfBestDetection()
    {
      return m_idxBestDetection;
    }

    int Detector::getPatchIdxOfDetection(int detectionIdx)
    {
      return m_idxDetections[detectionIdx];
    }

    BoostingTracker::BoostingTracker(ImageRepresentation* image, Rect initPatch, Rect validROI, int numBaseClassifier)
    {
      int numWeakClassifier = numBaseClassifier*10;
      bool useFeatureExchange = true;
      int iterationInit = 50;
      Size patchSize;
      patchSize = initPatch;

      this->validROI = validROI;

      classifier = new StrongClassifierDirectSelection(numBaseClassifier, numWeakClassifier, patchSize, useFeatureExchange, iterationInit);

      detector = new Detector (classifier);

      trackedPatch = initPatch;
      Rect trackingROI = getTrackingROI(2.0f);
      Size trackedPatchSize;
      trackedPatchSize = trackedPatch;
      Patches* trackingPatches = new PatchesRegularScan(trackingROI, validROI, trackedPatchSize, 0.99f);

      iterationInit = 50;
      for (int curInitStep = 0; curInitStep < iterationInit; curInitStep++)
      {
        printf ("\rinit tracker... %3.0f %% ", ((float)curInitStep)/(iterationInit-1)*100);	

        classifier->update (image, trackingPatches->getSpecialRect ("UpperLeft"), -1);
        classifier->update (image, trackedPatch, 1);
        classifier->update (image, trackingPatches->getSpecialRect ("UpperRight"), -1);
        classifier->update (image, trackedPatch, 1);
        classifier->update (image, trackingPatches->getSpecialRect ("LowerLeft"), -1);
        classifier->update (image, trackedPatch, 1);
        classifier->update (image, trackingPatches->getSpecialRect ("LowerRight"), -1);
        classifier->update (image, trackedPatch, 1);
      }

      confidence = -1;
      delete trackingPatches;

    }

    BoostingTracker::~BoostingTracker(void)
    {
      delete detector;
      delete classifier;
    }

    bool BoostingTracker::track(ImageRepresentation* image, Patches* patches)
    {
      //detector->classify (image, patches);
      detector->classifySmooth (image, patches);

      //move to best detection
      if (detector->getNumDetections() <=0)
      {
        confidence = 0;
        return false;
      }

      trackedPatch = patches->getRect (detector->getPatchIdxOfBestDetection ());
      confidence  = detector->getConfidenceOfBestDetection ();

      classifier->update (image, patches->getSpecialRect ("UpperLeft"), -1);
      classifier->update (image, trackedPatch, 1);
      classifier->update (image, patches->getSpecialRect ("UpperRight"), -1);
      classifier->update (image, trackedPatch, 1);
      classifier->update (image, patches->getSpecialRect ("UpperLeft"), -1);
      classifier->update (image, trackedPatch, 1);
      classifier->update (image, patches->getSpecialRect ("LowerRight"), -1);
      classifier->update (image, trackedPatch, 1);

      return true;
    }

    Rect BoostingTracker::getTrackingROI(float searchFactor)
    {
      Rect searchRegion;

      searchRegion = trackedPatch*(searchFactor);
      //check
      if (searchRegion.upper+searchRegion.height > validROI.height)
        searchRegion.height = validROI.height-searchRegion.upper;
      if (searchRegion.left+searchRegion.width > validROI.width)
        searchRegion.width = validROI.width-searchRegion.left;

      return searchRegion;
    }

    float BoostingTracker::getConfidence()
    {
      return confidence/classifier->getSumAlpha();
    }

    Rect BoostingTracker::getTrackedPatch()
    {
      return trackedPatch;
    }

    Point2D BoostingTracker::getCenter()
    {
      Point2D center;
      center.row = trackedPatch.upper + trackedPatch.height/2 ;
      center.col =  trackedPatch.left +trackedPatch.width/2 ;
      return center;
    }

    SemiBoostingTracker::SemiBoostingTracker(ImageRepresentation* image, Rect initPatch, Rect validROI, int numBaseClassifier)
    {
      int numWeakClassifier = 100;
      bool useFeatureExchange = true;
      int iterationInit = 50;
      Size patchSize;
      patchSize = initPatch;

      this->validROI = validROI;

      //	classifierOff = new StrongClassifierDirectSelection(numBaseClassifier, numBaseClassifier*10, patchSize, useFeatureExchange, iterationInit);
      classifierOff = new StrongClassifierStandardSemi(numBaseClassifier, numWeakClassifier, patchSize, useFeatureExchange, iterationInit);
      classifier = new StrongClassifierStandardSemi(numBaseClassifier, numWeakClassifier, patchSize, useFeatureExchange, iterationInit);  

      detector = new Detector (classifier);

      trackedPatch = initPatch;
      Rect trackingROI = getTrackingROI(2.0f);
      Size trackedPatchSize;
      trackedPatchSize = trackedPatch;
      Patches* trackingPatches = new PatchesRegularScan(trackingROI, validROI, trackedPatchSize, 0.99f);

      iterationInit = 50;
      for (int curInitStep = 0; curInitStep < iterationInit; curInitStep++)
      {
        printf ("\rinit tracker... %3.0f %% ", ((float)curInitStep)/(iterationInit-1)*100);	
        classifier->updateSemi (image, trackingPatches->getSpecialRect ("UpperLeft"), -1);
        classifier->updateSemi (image, trackedPatch, 1);
        classifier->updateSemi (image, trackingPatches->getSpecialRect ("UpperRight"), -1);
        classifier->updateSemi (image, trackedPatch, 1);
        classifier->updateSemi (image, trackingPatches->getSpecialRect ("LowerLeft"), -1);
        classifier->updateSemi (image, trackedPatch, 1);
        classifier->updateSemi (image, trackingPatches->getSpecialRect ("LowerRight"), -1);
        classifier->updateSemi (image, trackedPatch, 1);
      }
      printf (" done.\n");

      //one (first) shot learning
      iterationInit = 50;
      for (int curInitStep = 0; curInitStep < iterationInit; curInitStep++)
      {
        printf ("\rinit detector... %3.0f %% ", ((float)curInitStep)/(iterationInit-1)*100);

        classifierOff->updateSemi (image, trackedPatch, 1);
        classifierOff->updateSemi (image, trackingPatches->getSpecialRect ("UpperLeft"), -1);
        classifierOff->updateSemi (image, trackedPatch, 1);
        classifierOff->updateSemi (image, trackingPatches->getSpecialRect ("UpperRight"), -1);
        classifierOff->updateSemi (image, trackedPatch, 1);
        classifierOff->updateSemi (image, trackingPatches->getSpecialRect ("LowerLeft"), -1);
        classifierOff->updateSemi (image, trackedPatch, 1);
        classifierOff->updateSemi (image, trackingPatches->getSpecialRect ("LowerRight"), -1);
      }

      delete trackingPatches;

      confidence = -1;
      priorConfidence = -1;

    }

    SemiBoostingTracker::~SemiBoostingTracker(void)
    {
      delete detector;
      delete classifier;
    }

    bool SemiBoostingTracker::track(ImageRepresentation* image, Patches* patches)
    {
      //detector->classify(image, patches);
      detector->classifySmooth(image, patches);

      //move to best detection
      if (detector->getNumDetections() <=0 )
      {
        confidence = 0;
        priorConfidence = 0;
        return false;
      }

      trackedPatch = patches->getRect (detector->getPatchIdxOfBestDetection ());
      confidence = detector->getConfidenceOfBestDetection ();

      float off;

      //updates
      /*int numUpdates = 10;
      Rect tmp;
      for (int curUpdate = 0; curUpdate < numUpdates; curUpdate++)
      {
      tmp = patches->getSpecialRect ("Random");
      off = classifierOff->eval(image, tmp)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, tmp, off);

      priorConfidence = classifierOff->eval(image, trackedPatch)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, trackedPatch, priorConfidence);

      }*/

      Rect tmp = patches->getSpecialRect ("UpperLeft");
      off = classifierOff->eval(image, tmp)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, tmp, off);

      priorConfidence = classifierOff->eval(image, trackedPatch)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, trackedPatch, priorConfidence);

      tmp = patches->getSpecialRect ("LowerLeft");
      off = classifierOff->eval(image, tmp)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, tmp, off);

      priorConfidence = classifierOff->eval(image, trackedPatch)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, trackedPatch, priorConfidence);

      tmp = patches->getSpecialRect ("UpperRight");
      off = classifierOff->eval(image, tmp)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, tmp, off);	

      priorConfidence = classifierOff->eval(image, trackedPatch)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, trackedPatch, priorConfidence);

      tmp = patches->getSpecialRect ("LowerRight");
      off = classifierOff->eval(image, tmp)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, tmp, off);

      priorConfidence = classifierOff->eval(image, trackedPatch)/classifierOff->getSumAlpha();
      classifier->updateSemi (image, trackedPatch, priorConfidence);

      return true;
    }

    Rect SemiBoostingTracker::getTrackingROI(float searchFactor)
    {
      Rect searchRegion;

      searchRegion = trackedPatch*(searchFactor);
      //check
      if (searchRegion.upper+searchRegion.height > validROI.height)
        searchRegion.height = validROI.height-searchRegion.upper;
      if (searchRegion.left+searchRegion.width > validROI.width)
        searchRegion.width = validROI.width-searchRegion.left;

      return searchRegion;
    }

    float SemiBoostingTracker::getConfidence()
    {
      return confidence/classifier->getSumAlpha();
    }

    float SemiBoostingTracker::getPriorConfidence()
    {
      return priorConfidence;
    }

    Rect SemiBoostingTracker::getTrackedPatch()
    {
      return trackedPatch;
    }

    Point2D SemiBoostingTracker::getCenter()
    {
      Point2D center;
      center.row = trackedPatch.upper + trackedPatch.height/2 ;
      center.col = trackedPatch.left +trackedPatch.width/2 ;
      return center;
    }

  }
}