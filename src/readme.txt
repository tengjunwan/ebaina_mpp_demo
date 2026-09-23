Notice:
1. If the module parameter "mem_process_isolation" of the MMZ is set to "1", the process isolation attribute of the MMZ buffer is enabled.
When you run the sample and then run the tools, tools may fail to run due to process isolation problems. In consideration of this,
the "share all" operation is reserved in the sample, you can set "MEM_SHARE" to "y" during compilation to enable this function,
such as "make MEM_SHARE=y".

2. Due to the limitation of the DDR size, the following sample list cannot run on Hi3516CV610_10B or Hi3516CV608. If running is needed, it is recommended to reduce the number of VB or enable wrap.
    sample_vie (sample_vie 0/8 VPSS_OFFLINE mode can not run, sample_vie 1/7 only run VI_ONLINE_VPSS_ONLINE, sample_vie 2/3/4/5/6/9 cannot run)
    sample_venc(sample_venc 6/7/8/9/10 cannot run)
    sample_quick_start
    sample_uvc
    sample_calcflicker
    sample_svp_npu_main (sample_svp_npu_main 4/a cannot run)
    sample_ive_main (sample_ive_main 6/7 cannot run)
    sample_smart_ae
    sample_svp_svc

3. Hi3516CV608 not supports WDR mode, so the following samples cannot run:
    sample_vie 1/7/9

4. HY006_3814_0011_MIPI_4M_30FPS_10BIT does not support WDR mode, so the sample about WDR mode cannot run. For example, sample_vie 1/7/9.
However, the configuration related to the WDR mode will be retained.
If a sensor supports WDR mode, the configuration should be adapted according to HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1.

5. Hi3516CV608 supports only sns_type=HY006_3814_0011_MIPI_3M_30FPS_10BIT, which is cropped from HY006_3814_0011_MIPI_4M_30FPS_10BIT through the MIPI_RX.

6. HY006_3814_0011_MIPI_2M_30FPS_10BIT is only compatible with sample_kol. If the user need to use HY006_3814_0011_MIPI_2M_30FPS_10BIT and canot run,
some simple adaptations are required.

7. HY006_3814_0011_MIPI_3M_30FPS_10BIT is only compatible with Hi3516CV608. If the user need to use HY006_3814_0011_MIPI_3M_30FPS_10BIT and canot run,
some simple adaptations are required.

