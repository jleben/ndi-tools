#include <csignal>
#include <cstddef>
#include <cstdio>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstring>
#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>

#ifdef _WIN32
#include <windows.h>

#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64

#endif

#include <Processing.NDI.Lib.h>

using namespace std;

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int)
{	exit_loop = true;
}

class Widget : public QWidget
{
public:
    Widget()
    {
        d_thread = std::thread(&Widget::work, this);
    }

    ~Widget()
    {
        d_stop = true;
        d_thread.join();
    }

private:
    void work()
    {
        // Not required, but "correct" (see the SDK documentation.
        if (!NDIlib_initialize())
        {	// Cannot run NDI. Most likely because the CPU is not sufficient (see SDK documentation).
            // you can check this directly with a call to NDIlib_is_supported_CPU()
            printf("Cannot run NDI.");
            return;
        }

        // Create a finder
        const NDIlib_find_create_t NDI_find_create_desc; /* Use defaults */
        NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
        if (!pNDI_find) return;

        // We wait until there is at least one source on the network
        uint32_t no_sources = 0;
        const NDIlib_source_t* p_sources = NULL;
        while (!exit_loop && !no_sources)
        {	// Wait until the sources on the nwtork have changed
            NDIlib_find_wait_for_sources(pNDI_find, 1000);
            p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
        }

        // We need at least one source
        if (!p_sources) return;

        // We now have at least one source, so we create a receiver to look at it.
        // We tell it that we prefer YCbCr video since it is more efficient for us. If the source has an alpha channel
        // it will still be provided in BGRA
        NDIlib_recv_create_v3_t NDI_recv_create_desc;
        NDI_recv_create_desc.source_to_connect_to = p_sources[0];
        NDI_recv_create_desc.p_ndi_name = "Example NDI Receiver";
        NDI_recv_create_desc.color_format = NDIlib_recv_color_format_RGBX_RGBA;

        NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3(&NDI_recv_create_desc);
        if (!pNDI_recv) return;

        // Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
        NDIlib_find_destroy(pNDI_find);

        // We are now going to mark this source as being on program output for tally purposes (but not on preview)
        NDIlib_tally_t tally_state;
        tally_state.on_program = true;
        tally_state.on_preview = true;
        NDIlib_recv_set_tally(pNDI_recv, &tally_state);

        // Enable Hardwqre Decompression support if this support has it. Please read the caveats in the documentation
        // regarding this. There are times in which it might reduce the performance although on small stream numbers
        // it almost always yields the same or better performance.
        NDIlib_metadata_frame_t enable_hw_accel;
        enable_hw_accel.p_data = "<ndi_hwaccel enabled=\"true\"/>";
        NDIlib_recv_send_metadata(pNDI_recv, &enable_hw_accel);

        while (!d_stop)
        {
            // The descriptors
            NDIlib_video_frame_v2_t video_frame;
            NDIlib_audio_frame_v2_t audio_frame;
            NDIlib_metadata_frame_t metadata_frame;

            switch (NDIlib_recv_capture_v2(pNDI_recv, &video_frame, &audio_frame, &metadata_frame, 500))
            {
            // No data
            case NDIlib_frame_type_none:
                printf("No data received.\n");
                break;

            // Video data
            case NDIlib_frame_type_video:
            {
                printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);

                d_mutex.lock();

                if (video_frame.xres != d_image.width() || video_frame.yres != d_image.height())
                {
                    d_image = QImage(video_frame.xres, video_frame.yres, QImage::Format_RGBA8888);
                }

                auto source = video_frame.p_data;

                for (int y = 0; y < video_frame.yres; ++y)
                {
                    auto dest = d_image.scanLine(y);
                    std::memcpy(dest, source, video_frame.xres * 4);
                    source += video_frame.line_stride_in_bytes;
                }

                d_mutex.unlock();

                NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);

                update();

                break;
            }

            // Audio data
            case NDIlib_frame_type_audio:
                printf("Audio data received (%d samples).\n", audio_frame.no_samples);
                NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
                break;

            // Meta data
            case NDIlib_frame_type_metadata:
                printf("Meta data received.\n");
                NDIlib_recv_free_metadata(pNDI_recv, &metadata_frame);
                break;

            // There is a status change on the receiver (e.g. new web interface)
            case NDIlib_frame_type_status_change:
                printf("Receiver connection status changed.\n");
                break;

            // Everything else
            default:
                break;
            }
        }

        // Destroy the receiver
        NDIlib_recv_destroy(pNDI_recv);

        // Not required, but nice
        NDIlib_destroy();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);

        d_mutex.lock();

        if (d_image.isNull())
        {
            d_mutex.unlock();
            return;
        }

        auto image = d_image.scaled(size(), Qt::KeepAspectRatio);

        painter.drawImage(QPoint(0,0), image);

        d_mutex.unlock();
    }

    std::thread d_thread;
    atomic<bool> d_stop { false };
    mutex d_mutex;
    QImage d_image;
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Widget w;
    w.show();

    return app.exec();
}

