1. Compile the SPI video output sample.
  Download the open-source software ffmpeg-7.1.tar.gz and save it to open_source/ffmpeg/ffmpeg-7.1.tar.gz.
  cd smp/a7_linux/source/mpp/sample/spi_lcd Generating sample_spi_lcd
  make clean;make
 The sample generated after compilation is stored in smp/a7_linux/source/mpp/sample/spi_lcd/sample_spi_lcd.


2. Run the SPI video output sample.
2.1. Pin multiplexing
  SPI pin multiplexing involves SPI_SCL, SPI_MISO, and SPI_CSN. GPIO pin multiplexing involves DC (data/cmd) and TE (Tear Effect).
2.2. The GPIO and SPI clocks are enabled by default in Linux. To compile the Linux kernel with small configurations such as fast start, you need to confirm the following:
2.3. Compile peripheral spi_xxx screen driver by yourself, first load spi_xxx.ko, and then load ot_spi_dma_transfer.ko
  ot_spi_dma_transfer.ko depends on other drivers such as osal and mmz. You need to load the dependent driver before loading ot_spi_dma_transfer.ko.
2.4. Run the sample.
  ./sample_spi_lcd 0 capture video data from camera  to the lcd
  ./sample_spi_lcd 1 Decode the data from file to the lcd. The decoding depends on the H.265 raw stream file input.h265.
