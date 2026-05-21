# Holiday Pixel Lights project status 4/12/2026   
  
Project repository:  
[https://github.com/bobh/holiday-pixel-display](https://github.com/bobh/holiday-pixel-display)  
  
software/firmware The software is developed with two CSC efforts. The MacBook Pro (MBP) part and the MacBook Air (MBA) part. The CSCs and their computers are physically separated.  
  
1A. The MBP component is for the iPhone and developed in Xcode with a CLI version of Claude Code.  
  
1B.  The MBA component is for the Arduino Nano 33 BLE Sense rev 2 and is developed in Visual Studio Code using the Claude Code Extension. The Arduino run environment is used with the Setup() and Loop() architecture and the Arduino Nano build chain.  
  
1C. The hardware effort: a hardware PCB was developed and fabricated. It has gone through initial checkout and fundamentally works but hardware problems were discovered. The I2C lines (SCA and SCL) were swapped at the NVRAM part FM24CL16B. Various attempts to remap (and swap) the pin functionality on the Arduino Nano 33 BLE device have failed thus causing the current interruption in development. A new spin of the hardware PCB is under consideration.  
Hardware not verified is the TPS61023 Battery Boost Converter part and external LiPo battery operation. Operation to this point has been on an external AC 5 VDC “Brick” supply. Calibration of the LiPo battery discharge characteristics (as monitored on the Arduino Nano A to D converter GPIO A0 through a two 100K 1% voltage divider) and disabling of the battery with Arduino Nano GPIO D5 when voltage is too low must still be verified.  
  
Also, the BLE remote LED effect selection is not working. The 3rd party nRF Connect iPhone BLE diagnostic app sees the MBA Arduino Nano CSC advertising and can connect but the MBP Central iPhone app cannot connect and remains in the scanning mode. A suggested solution to this was created on the MBA at /Users/bobh/Desktop/Projects/HolidayDisplay/BLEPeripheral.ino but has not been verified and not been pushed to the repository.   
  
Further development is suspended awaiting a plan to continue forward.  
  
  
  
  
