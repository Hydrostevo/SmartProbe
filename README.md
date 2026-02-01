# SmartProbe
Camera based remote crop monitoring using and ESP32 camera.  The solution will provide images of a 1m square quadrat with image capture based on a timelapse.  The images will be sent to an Azure server where image processing will be carried out using OpenCV (probably).

# Device selection
The Solution will be developed in parallel across three possible devices...
| Device                     | Camera  | GPS | Mobile | Data capture          | Comment                                                        |
| -------------------------- | ------- | --- | ------ | --------------------- | -------------------------------------------------------------- |
| ESP32Cam + SIM800L         | OV2640  | --- | ------ | Azure + Local SD card | Limited to 2G network areas and being phased out across the UK |
| Waveshare ESP32S3SIM7670G  | OV2640  | --- | ------ | Azure + Local SD card |                                                                |
| ESP32S3N16R8               | OV2640  | --- | No     | Local SD card         |                                                                |

+++++
