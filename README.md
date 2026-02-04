# SmartProbe
Camera based remote crop monitoring using and ESP32 module with camera.  The solution will provide images of a 1m<sup>2</sup> quadrat with image capture based on a timelapse.  The images will be sent to an Azure server where image processing will be carried out using OpenCV (probably).

## Device selection
The Solution will be developed in parallel across three possible devices...
| Device                     | Camera  | GPS | Mobile         | Data capture          | Comment                                                                  |
| -------------------------- | ------- | --- | -------------- | --------------------- | ------------------------------------------------------------------------ |
| ESP32Cam + SIM800L         | OV2640  | --- | 2G - *SIM800L  | Azure + Local SD card |                                                                          |
| [Waveshare ESP32S3SIM7670G](https://www.waveshare.com/wiki/ESP32-S3-SIM7670G-4G#Hardware_Description)  | OV2640  | --- | 4G - SIM7670G  | Azure + Local SD card |  |
| ESP32S3N16R8               | OV2640  | --- | None           | Local SD card         | Stand alone design with image downlaod via local Wenserver               |
| ESP32Cam                   | OV2640  | --- | None           | Local SD card         | Stand alone design with image downlaod via local Wenserver               |

*  SIM8000L is limited to 2G network areas which are being phased out across the UK

## Problem statement
Sugar Beet crop growth needs to be frequently monitored from drilling (planting) to 12 true leaves (adolecence).  This requires a physical visit to the field on a frequent basis to observe and photograph the development stage.  At the same time Leaf Area index and ground coverage are recorded using the [Canopeo](https://canopeoapp.com/) app.


## Credits and honorable mentions
Richard Smart for such a bat $h!t crazy idea and 320 years of Agronomy experience
Lee Arbon for buttery smooth code and being an Azure God incarnate
Simon Robinson for solution development and misguided encoragement
Elliot Wheeler for image manipulation and redefining the concepts of reality
