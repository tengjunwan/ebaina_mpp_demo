Compile:
	only compile uvc default, if need uac, please change the value of "UAC_COMPILE" into "y" in Makefile.perf and Makefile.v4l2 .
	$ make clean
	$ make


There are 4 data input mode for sample_uvc to select:
1. OT_UVC_MPP_BIND_UVC: for venc stream or yuv frame, VENC bind UVC or VPSS/VI bind UVC, stream/yuv data flows in the kernel space;
2. OT_UVC_SEND_VENC_STREAM: user get VENC stream from venc channel, save or send out by network, then send venc stream to UVC by ss_mpi_uvc_send_stream();
3. OT_UVC_SEND_YUV_FRAME: user get YUV frame from VI/VPSS, then send it to UVC by ss_mpi_uvc_send_frame();
4. OT_UVC_SEND_USER_STREAM: user have private buffer that contains encoded stream, send it to UVC by ss_mpi_uvc_send_user_stream(), this is not for YUV frame, we can use VI/VPSS bind UVC for YUV frame.


Usage & Command Args
	for sample_uvc, it send frame/stream by ss_mpi_uvc_xxx which provided by ot_uvc.ko and uvc library.
	Usage1: ./sample_uvc -h
		-h  --print help information
	Usage2: ./sample_uvc [DEVICE_COUNT] [DATA_MODE]
		DEVICE_COUNT: uvc device(also for uvc channel) count, should in [1, 2], should consistent with ConfigUVC script.
		DATA_MODE: uvc data mode, should in [0, 2], will be stored in uvc_media.c:g_data_input_mode.
			0 OT_UVC_MPP_BIND_UVC
			1 OT_UVC_SEND_VENC_STREAM/OT_UVC_SEND_YUV_FRAME
			2 OT_UVC_SEND_USER_STREAM

	for sample_uvc_v4l2, it is pure a V4L2 application and has nothing to do with ss_mpi_uvc_xxx or ot_uvc.ko.
	Usage1 : ./sample_uvc_v4l2 -h
			-h  --print help information
	Usage2 : ./sample_uvc_v4l2 [DEVICE_COUNT]
			DEVICE_COUNT: max uvc device count, should in [1, 2]

About 2 UVC Devices
	The sample support max 2 uvc devices, but don't support still image at the same time.
	In sample_uvc, UVC chn 0 serve for /dev/video0, UVC chn 1 serve for /dev/video1, sample_uvc_v4l2 don't use UVC chn.

About Memory
	The sample support max 2 uvc devices, use max 2 HY006_3814_0011(2560*1440) sensor and run typically in 64MB OS memory and 64M MMZ memory,
	if you have a different RAM memory size or image sensor input size, or found some problems like "get vb fail", "malloc fail",
	you need to adjust the memory configuration by the following methods:
	1. Resize the OS memory int bootargs and MMZ memory(mmz_size and mmz_start in ko/load3516cv6XX/load3516cv6XX...),  don't overlap.
	2. cut off some oversize resolution, for example, 
		export MJPEG="360p 720p 1080p 1440p"
		can be modified to
		export MJPEG="360p 720p 1080p"
	3. change the vb block count and size in uvc_media.c(sample_uvc) or uvc_media_v4l2.c(sample_uvc_v4l2).