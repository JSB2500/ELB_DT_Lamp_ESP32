# ELB Lamp

ESP32:  
• ESP32 module used: ESP32-WROOM-32.  
• ESP32 chip in module: ESP32-D0WDQ6 (revision v1.0).  
• Module has 4MB flash ROM.  

Building:  
• idf.py all  

Programming:  
• Connect ESP32 USB connector to PC.  
• Power lamp with 12V.  
• Press and hold BOOT button.  
• Run "idf --port COM4 flash".  
• To program firmware from the Repository folders, use esptool.py or "ESP FLASH DOWNLOAD TOOL" (GUI) to program firmware bin file to address 0x10000 (I think that's what "idf.py flash" does).  

Monitoring:  
• Press and hold BOOT button.  
• Run "idf --port COM4 monitor".  
