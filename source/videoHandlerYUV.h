/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VIDEOHANDLERYUV_H
#define VIDEOHANDLERYUV_H

#include "typedef.h"
#include <map>
#include <QString>
#include <QSize>
#include <QList>
#include <QPixmap>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QMutex>
#include "videoHandler.h"
#include "ui_videoHandlerYUV.h"

/** The YUVSource can be anything that provides raw YUV data. This can be a file or any kind of decoder or maybe a network source ...
  * A YUV sources supports handling of YUV data and can return a specific frame as a pixmap by calling getOneFrame.
  * So this class can perform all conversions from YUV to RGB.
  */
class videoHandlerYUV : public videoHandler, Ui_videoHandlerYUV
{
  Q_OBJECT

public:
  videoHandlerYUV();
  virtual ~videoHandlerYUV();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() { return (frameSize.isValid() && srcPixelFormat != "Unknown Pixel Format"); }

  // Return the YUV values for the given pixel
  virtual ValuePairList getPixelValues(QPoint pixelPos) Q_DECL_OVERRIDE;
  // For the difference item: Return values of this item, the other item and the difference at
  // the given pixel position. Call playlistItemVideo::getPixelValuesDifference if the given
  // item cannot be cast to a playlistItemYuvSource.
  virtual ValuePairList getPixelValuesDifference(QPoint pixelPos, videoHandler *item2) Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource
  // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
  // we will use the playlistItemVideo::calculateDifference function to calculate the difference
  // using the RGB values.
  virtual QPixmap calculateDifference(videoHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference) Q_DECL_OVERRIDE;

  // Get the number of bytes for one YUV frame with the current format
  virtual qint64 getBytesPerYUVFrame() { return srcPixelFormat.bytesPerFrame(frameSize); }

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can guess
  // the remaining values. The rate value is set if a matching format could be found.
  // If the sub format is "444" we will assume 4:4:4 input. Otherwise 4:2:0 will be assumed.
  void setFormatFromSize(QSize size, int bitDepth, qint64 fileSize, int rate, int subFormat);

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw YUV data.
  // If a file size is given, it is tested if the YUV format and the file size match.
  void setFormatFromCorrelation(QByteArray rawYUVData, qint64 fileSize=-1);

  // Create the yuv controls and return a pointer to the layout.
  // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createYuvVideoHandlerControls(QWidget *parentWidget, bool yuvFormatFixed=false);

  // Get the name of the currently selected YUV pixel format
  QString getSrcPixelFormatName() { return srcPixelFormat.name; }
  // Set the current yuv format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  void setSrcPixelFormatName(QString name, bool emitSignal=false);

  // When loading a videoHandlerYUV from playlist file, this can be used to set all the parameters at once
  void loadValues(QSize frameSize, indexRange startEndFrame, int sampling, double frameRate, QString sourcePixelFormat);

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for the given range of pixels.
  // Overridden from playlistItemVideo. This is a YUV source, so we can draw the YUV values.
  virtual void drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2=NULL) Q_DECL_OVERRIDE;

#if SSE_CONVERSION
  // A 16 bit aligned byte array for the raw YUV data
  byteArrayAligned rawYUVData;
#else
  // A buffer with the raw YUV data (this is filled if signalRequesRawYUVData() is emitted)
  QByteArray rawYUVData;

  // The buffer of the raw YUV data of the current frame (and its frame index)
  // Before using the currentFrameRawYUVData, you have to check if the currentFrameRawYUVData_frameIdx is correct. If not,
  // you have to call loadFrame(idx) to load the frame and set it correctly.
  QByteArray currentFrameRawYUVData;
  int        currentFrameRawYUVData_frameIdx;
#endif
  int rawYUVData_frameIdx;

signals:

  // This signal is emitted when the handler needs the raw YUV data for a specific frame. After the signal
  // is emitted, the requested data should be in rawYUVData and rawYUVData_frameIdx should be identical to
  // frameIndex.
  void signalRequesRawYUVData(int frameIndex);

protected:

  // The interpolation mode for conversion from non YUV444 to YUV 444
  typedef enum
  {
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
  } InterpolationMode;
  InterpolationMode interpolationMode;

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayY,
    DisplayCb,
    DisplayCr
  } ComponentDisplayMode;
  ComponentDisplayMode    componentDisplayMode;

  // How to convert from YUV to RGB
  /*
  kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
  R = Y                  + V*(1-Kr)
  G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
  B = Y + U*(1-Kb)

  To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled by 255/219 for Y and 255/112 for U/V
  In this software color conversion is performed with 16bit precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
  */
  typedef enum {
    YUVC709ColorConversionType,
    YUVC601ColorConversionType,
    YUVC2020ColorConversionType
  } YUVCColorConversionType;
  YUVCColorConversionType yuvColorConversionType;

    // This struct defines a specific yuv format with all properties like pixels per sample, subsampling of chroma
  // components and so on.
  struct yuvPixelFormat
  {
    // The default constructor (will create an "Unknown Pixel Format")
    yuvPixelFormat() : name("Unknown Pixel Format") , bitsPerSample(0) , bitsPerPixelDenominator(0)
                   , subsamplingHorizontal(0), subsamplingVertical(0), planar(false), bytePerComponentSample(1) {}
    // Convenience constructor that takes all the values.
    yuvPixelFormat(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator,
                   int subsamplingHorizontal, int subsamplingVertical, bool planar, int bytePerComponentSample = 1)
                   : name(name) , bitsPerSample(bitsPerSample), bitsPerPixelNominator(bitsPerPixelNominator)
                   , bitsPerPixelDenominator(bitsPerPixelDenominator), subsamplingHorizontal(subsamplingHorizontal)
                   , subsamplingVertical(subsamplingVertical), planar(planar), bytePerComponentSample(bytePerComponentSample) {}
    bool operator==(const yuvPixelFormat& a) const { return name == a.name; } // Comparing names should be enough since you are not supposed to create your own yuvPixelFormat instances anyways.
    bool operator==(const QString& a) const { return name == a; }
    bool operator!=(const QString& a) const { return name != a; }
    // Get the number of bytes for a frame with this yuvPixelFormat and the given size
    qint64 bytesPerFrame( QSize frameSize );
    QString name;
    int bitsPerSample;
    int bitsPerPixelNominator;
    int bitsPerPixelDenominator;
    int subsamplingHorizontal;
    int subsamplingVertical;
    bool planar;
    int bytePerComponentSample;
  };

  // The currently selected yuv format
  yuvPixelFormat srcPixelFormat;

  // A (static) convenience QList class that handels all supported yuvPixelFormats
  class YUVFormatList : public QList<yuvPixelFormat>
  {
  public:
    // Default constructor. Fill the list with all the supported YUV formats.
    YUVFormatList();
    // Get all the YUV formats as a formatted list (for the dropdonw control)
    QStringList getFormatedNames();
    // Get the yuvPixelFormat with the given name
    yuvPixelFormat getFromName(QString name);
  };
  static YUVFormatList yuvFormatList;

  // Parameters for the YUV transformation (like scaling, invert, offset)
  int lumaScale, lumaOffset, chromaScale, chromaOffset;
  bool lumaInvert, chromaInvert;

  // Temporaray buffers for intermediate conversions
#if SSE_CONVERSION
  byteArrayAligned tmpBufferYUV444;
  byteArrayAligned tmpBufferRGB;
#else
  QByteArray tmpBufferYUV444;
  QByteArray tmpBufferRGB;
  // Caching
  QByteArray tmpBufferYUV444Caching;
  QByteArray tmpBufferRGBCaching;
  QByteArray tmpBufferRawYUVDataCaching;
#endif

  // Get the YUV values for the given pixel.
  virtual void getPixelValue(QPoint pixelPos, unsigned int &Y, unsigned int &U, unsigned int &V);

  // Load the given frame and convert it to pixmap. After this, currentFrameRawYUVData and currentFrame will
  // contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex) Q_DECL_OVERRIDE;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and currentFrame)
  // will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache) Q_DECL_OVERRIDE;

  // Do we need to apply any transform to the raw YUV data before conversion to RGB?
  bool yuvMathRequired() { return lumaScale != 1 || lumaOffset != 125 || chromaScale != 1 || chromaOffset != 128 || lumaInvert || chromaInvert || componentDisplayMode != DisplayAll; }

private:

  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

  // Convert from YUV (which ever format is selected) to pixmap (RGB-888)
  void convertYUVToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer);

#if SSE_CONVERSION
  // Convert one frame from the current pixel format to YUV444
  void convert2YUV444(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
  // Apply transformations to the luma/chroma components
  void applyYUVTransformation(byteArrayAligned &sourceBuffer);
  // Convert one frame from YUV 444 to RGB
  void convertYUV4442RGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
  // Directly convert from YUV 420 to RGB (do not apply YUV math)
  void convertYUV420ToRGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
#else
  // Convert one frame from the current pixel format to YUV444
  void convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  // Apply transformations to the luma/chroma components
  void applyYUVTransformation(QByteArray &sourceBuffer);
  // Convert one frame from YUV 444 to RGB
  void convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  // Directly convert from YUV 420 to RGB (do not apply YUV math)
  void convertYUV420ToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer);
#endif

  void yuv420_to_argb8888(quint8 *yp, quint8 *up, quint8 *vp,
                          quint32 sy, quint32 suv,
                           int width, int height,
                           quint8 *rgb, quint32 srgb );

  bool controlsCreated;    ///< Have the controls been created already?

  // Only one thread at a time should changed rawYUVData
  QMutex rawYUVDataMutex;

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();

};

#endif // VIDEOHANDLERYUV_H
