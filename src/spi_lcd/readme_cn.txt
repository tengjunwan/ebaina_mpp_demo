1. 编译SPI视频输出sample
  下载 open-source 下ffmpeg官网软件包 ffmpeg-7.1.tar.gz, 存到目录 open_source/ffmpeg/ffmpeg-7.1.tar.gz.
  去目录 smp/a7_linux/source/mpp/sample/spi_lcd 下执行 make clean;make 命令编译, 生成可执行文件 sample_spi_lcd

2.运行SPI视频输出sample
2.1. 管脚复用
  SPI 管脚复用包含SPI_SCL, SPI_MISO, SPI_CSN. GPIO 管脚复用包含DC (data/cmd), TE (Tear Effect).
2.2. GPIO 和 SPI 时钟默认已经在linux内核里使能, 编译linux内核可使用最简配置，例如编译快起。此外，需要如下配置操作
  2.2.1. 编译屏幕外设驱动 spi_xxx screen , 首先加载spi_xxx.ko, 然后加载ot_spi_dma_transfer.ko
  ot_spi_dma_transfer.ko 依赖其他驱动例如 osal and mmz. 需要先加载这些ko
  2.2.2. 执行sample.
  ./sample_spi_lcd 0 从sensor捕获视频送到lcd屏幕
  ./sample_spi_lcd 1 解码H.265 raw stream码流文件后送到lcd屏幕，依赖的码流文件是input.h265.
