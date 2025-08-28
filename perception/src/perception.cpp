// Created by: Francisco GENTILE
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <queue>
#include <list>
#include <fstream>
#include <thread>
#include <future>
#include <atomic>
#include <mutex> // std::mutex, std::unique_lock
#include <cmath>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <tuple>
#include <stdexcept>
#include <csignal>

#include "base64.hpp"

#include <sys/time.h>
#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <cuda_runtime_api.h>
#include <sys/timeb.h>
#include <yaml-cpp/yaml.h>
#include <json-glib/json-glib.h>
#include <mosquitto.h>
#include <sw/redis++/redis++.h>

#include "gstnvdsmeta.h"
#include "nvds_version.h"
#include "nvds_yml_parser.h"
#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvds_obj_encode.h"
#include "gst-nvmessage.h"
#include "nvdstracker.h"
#include "nvds_msgapi.h"
#include "nvdsmeta_schema.h"
#include "nvds_analytics_meta.h"

#include "args_parameters.hpp"
#include "Pipeline.hpp"
#include "perception.hpp"
#include "ObjInfo.hpp"

// This is necessary to avoid NVS_APP not defined error.
GST_DEBUG_CATEGORY(NVDS_APP);

using namespace g2f;
using namespace sw::redis;


typedef struct {
    AppCtx_t *appCtx;
    g2f::Pipeline *pipeline;
} ProbeData_t;


static bool terminate_program = false;


std::string getCurrentTimestampAsString() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&time);
    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}



static gboolean
check_performance(gpointer data)
{
    g2f::Pipeline *pipeline = (g2f::Pipeline *)data;

    // // Check if terminate program was requested
    // if (terminate_program) return false;

    for (int i = 0; i < pipeline->num_sources_; i++)
    {
        // Get current timestamp and format it to a string for saving into a file
        std::string time_str = getCurrentTimestampAsString();

        if ((pipeline->streams_[i].source_bin != nullptr)) {
            if (pipeline->streams_[i].frameCount == 0) {
                g_print("%s \x1b[31;1mStream %d:\x1b[0m Frame Count=\x1b[31;1m%d\x1b[0m\r\n", time_str.c_str(), i, pipeline->streams_[i].frameCount);
                pipeline->streams_[i].noFrameCount++;
                if (pipeline->streams_[i].noFrameCount > 20) {
                    g_print("\x1b[31;1mStream %d:\x1b[0m No frames for 20 seconds.\r\n", i);

                    // Save timestamp, stream id, description into a csv file called ver_logs.txt
                    std::ofstream ver_logs;
                    ver_logs.open("/opt/storage/ver_logs.txt", std::ios::app);
                    ver_logs << time_str << "," << i << "," << pipeline->streams_[i].description << "\n";
                    ver_logs.close();

                    switch (pipeline->streams_[i].reset) {
                    case StreamResetStates::NONE:
                    case StreamResetStates::DELETE_STREAM:
                        // Stop the source bin
                        if (pipeline->streams_[i].source_bin != nullptr) {
                            g_print("Stream %d (%s) is being DELETED from the pipeline.\r\n", i, pipeline->streams_[i].description.c_str());
                            pipeline->delete_source(pipeline->streams_[i]);

                            // Set the reset state to WAITING to re-add the stream
                            pipeline->streams_[i].reset = StreamResetStates::WAITING;
                        }
                        break;
                    case StreamResetStates::WAITING:
                        g_print("Stream %d (%s) is in WAITING state, preparing to re-add.\r\n", i, pipeline->streams_[i].description.c_str());
                        if (pipeline->streams_[i].source_bin != nullptr) {
                            if (pipeline->streams_[i].noFrameCount >= 30)
                                pipeline->streams_[i].reset = StreamResetStates::ADD_STREAM;
                        }
                        break;
                    case StreamResetStates::ADD_STREAM:
                        g_print("Stream %d (%s) is in ADD_STREAM state, re-adding to the pipeline.\r\n", i, pipeline->streams_[i].description.c_str());
                        // Add the source bin back to the pipeline
                        pipeline->streams_[i].source_bin = nullptr;
                        pipeline->add_source(
                            pipeline->streams_[i].uri,
                            pipeline->streams_[i].camera_id,
                            pipeline->streams_[i].pad_index,
                            pipeline->streams_[i].description,
                            true,
                            true);
                        break;
                    case StreamResetStates::CHANGE_STATE_TO_NULL:
                    case StreamResetStates::CHECK_FOR_STATE_NULL:
                    case StreamResetStates::INITIALITING:
                    case StreamResetStates::READY:
                        break;
                    }    
                }
            } else {
                g_print("Stream %d: Frame Count=%d\r\n", i, pipeline->streams_[i].frameCount);
                pipeline->streams_[i].noFrameCount = 0;
            }
        }
        pipeline->streams_[i].frameCount = 0;
    }
    return true;
}

// static gboolean
// check_performance(gpointer data)
// {
//     g2f::Pipeline *pipeline = (g2f::Pipeline *)data;

//     // // Check if terminate program was requested
//     // if (terminate_program) return false;

//     for (int i = 0; i < pipeline->num_sources_; i++)
//     {
//         // Get current timestamp and format it to a string for saving into a file
//         std::string time_str = getCurrentTimestampAsString();

//         if (pipeline->streams_[i].frameCount == 0) {
//             // Save timestamp, stream id, description into a csv file called ver_logs.txt
//             std::ofstream ver_logs;
//             ver_logs.open("/opt/storage/ver_logs.txt", std::ios::app);
//             ver_logs << time_str << "," << i << "," << pipeline->streams_[i].description << "\n";
//             ver_logs.close();

//             g_print("%s \x1b[31;1mStream %d:\x1b[0m Frame Count=\x1b[31;1m%d\x1b[0m\r\n", time_str.c_str(), i, pipeline->streams_[i].frameCount);
//             pipeline->streams_[i].noFrameCount++;
//             if (pipeline->streams_[i].noFrameCount > 20) {
//                 g_print("\x1b[31;1mStream %d:\x1b[0m No frames for 20 seconds. Stopping stream.\r\n", i);
//                 pipeline->streams_[i].reset = StreamResetStates::DELETE_STREAM;
//             }
//         } else {
//             g_print("Stream %d: Frame Count=%d\r\n", i, pipeline->streams_[i].frameCount);
//             pipeline->streams_[i].noFrameCount = 0;
//         }
//         pipeline->streams_[i].frameCount = 0;
//     }

//     return true;
// }


void StreamCheckerTask(AppCtx_t *AppCtx) {
    int counter = 0;

    while (!terminate_program) {
        // Lock GMutex
        if (g_mutex_trylock(&AppCtx->lock_streams_info)) {
            // bool any_frame = false;

            // for (int i = 0; i < AppCtx->num_sources; i++)
            // {
            //     if (AppCtx->streams[i].source_bin != nullptr) {
            //         if (AppCtx->streams[i].frameCount != 0) {
            //             any_frame = true;
            //         }
            //     }
            // }

            // // Unlock Gmutex
            // g_mutex_unlock(&AppCtx->lock_streams_info);

            // if (any_frame) {
            //     counter = 0;
            // } else {
            //     if (++counter >= 30) {
            //         g_print("Timeout waiting for streams to be ready\r\n");
            //         terminate_program = true;
            //         // Quit Gstreamer loop
            //         g_main_loop_quit(AppCtx->loop);
            //         // Stop Gstreamer pipeline
            //         gst_element_set_state(AppCtx->pipeline, GST_STATE_NULL);
            //     }
            // }
        }
        if (!terminate_program) {
            // Sleep 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    g_print("StreamCheckerTask finished\r\n");
}


int main(int argc, char* argv[]) {
    GST_DEBUG_CATEGORY_INIT(NVDS_APP, "NVDS_APP", 0, NULL);

    gflags::SetUsageMessage("perception [OPTIONS] <config.yml file>");
    gflags::SetVersionString("1.0.0");
    // Parse command line arguments
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_h)
    {
        showUsage();
        return (0);
    }
    // Check input arguments
    if (argc != 2)
    {
        g_printerr("Usage: %s [OPTIONS] <config.yml file>\n", argv[0]);
        return -1;
    }

    // Parsing Arguments
    // =================
    // Threads in the Pool
    int n_threads = FLAGS_n_threads;
    if (n_threads > std::thread::hardware_concurrency()-1)
    {
        n_threads = std::thread::hardware_concurrency()-1;
        std::cout << "Requested Number of threads " << FLAGS_n_threads << " was adjusted to " << n_threads << " due to hardware." << std::endl;
    }
    // Sources YAML file
    std::string sources_yml_file = FLAGS_streams;
    // Use nvurisrcbin
    bool use_nvurisrcbin = FLAGS_use_nvurisrcbin;

    // Get CUDA properties
    int current_device = -1;
    cudaGetDevice(&current_device);
    struct cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, current_device);


    // Application context
    AppCtx_t AppCtx = {
        // Read configuration file
        .settings = YAML::LoadFile(argv[1]),
        .sources = YAML::LoadFile(sources_yml_file),
        .pool = nullptr,
        .mosq = nullptr,
        .obj_list = nullptr,
        .app_is_running = true,
        .save_frames = FLAGS_save_frames,
        .redis = nullptr,
        .redis_ttl = 0, //configs["redis"]["ttl_sec"].as<long long int>(),
    };
    // Summary
    g_print("\r\nPipeline Config File: \x1b[33;1m%s\x1b[0m\r\n", argv[1]);
    g_print("        Sources File: \x1b[33;1m%s\x1b[0m\r\n", sources_yml_file.c_str());
    g_print("         Save Frames: \x1b[33;1m%s\x1b[0m\r\n\r\n", AppCtx.save_frames ? "true" : "false");

    // Main variables
    GMainLoop *loop = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;
    g2f::Pipeline *pipeline = NULL;
    std::queue<g2f::ObjInfo> obj_info_queue;
    std::queue<g2f::FrameInfo> frame_info_queue;
    // Save in app context directly
    AppCtx.obj_list = &obj_info_queue;
    AppCtx.frame_list = &frame_info_queue;
    g_mutex_init(&AppCtx.lock_streams_info);

    // Create thread pool
    boost::asio::thread_pool pool(n_threads);
    AppCtx.pool = &pool;

    // Parsed YAML
    YAML::Node sources_list = YAML::LoadFile(sources_yml_file);


    // Output Directory preparation
    if (AppCtx.save_frames)
    {
        if (!prepareOutputDirectory(AppCtx.settings["output-directory"].as<std::string>(), (int)sources_list.size()))
        {
            // If it wasn't possible to create the output directory, disable saving frames
            AppCtx.save_frames = false;
        }
    }

    // Standard GStreamer initialization
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);


    // Print the version of the GStreamer library
    std::cout << std::endl;
    nvds_version_print();
    nvds_dependencies_version_print();
    std::cout << std::endl;


    // Redis
    // Configuration
    auto redis_config = AppCtx.settings["redis"];
    ConnectionOptions connection_options;
    connection_options.host = redis_config["host"].as<std::string>();
    connection_options.port = redis_config["port"].as<unsigned short int>();
    if (redis_config["user"].as<std::string>().length() > 0)
        connection_options.user = redis_config["user"].as<std::string>();
    if (redis_config["password"].as<std::string>().length() > 0)
        connection_options.password = redis_config["password"].as<std::string>();
    if (redis_config["db"].as<int>() >= 0)
        connection_options.db = redis_config["db"].as<int>();
    // Optional. Timeout before we successfully send request to or receive response from redis.
    // By default, the timeout is 0ms, i.e. never timeout and block until we send or receive successfuly.
    // NOTE: if any command is timed out, we throw a TimeoutError exception.
    connection_options.socket_timeout = std::chrono::milliseconds(redis_config["socket_timeout"].as<int>());
    //
    // Connection Pool options
    ConnectionPoolOptions pool_options;
    // Pool size, i.e. max number of connections.
    pool_options.size = redis_config["pool_size"].as<int>();
    // Optional. Max time to wait for a connection. 0ms by default, which means wait forever.
    // Say, the pool size is 3, while 4 threads try to fetch the connection, one of them will be blocked.
    pool_options.wait_timeout = std::chrono::milliseconds(redis_config["pool_wait_timeout"].as<int>());
    // Optional. Max lifetime of a connection. 0ms by default, which means never expire the connection.
    // If the connection has been created for a long time, i.e. more than `connection_lifetime`,
    // it will be expired and reconnected.
    pool_options.connection_lifetime = std::chrono::minutes(redis_config["pool_connection_lifetime"].as<int>());
    //
    // Redis object
    sw::redis::Redis redis = Redis(connection_options, pool_options);
    // Save in app context directly
    AppCtx.redis = &redis;
    // Save ttl in app context directly
    AppCtx.redis_ttl = redis_config["ttl_sec"].as<long long int>();



    // Connect to Mosquitto Broker
    mosquitto_lib_init();
    // Save in app context directly
    AppCtx.mosq = mosquitto_new(AppCtx.settings["mqtt-broker"]["conn-id"].as<std::string>().c_str(), true, NULL);
    // Connect to Mosquitto broker
    int ret = mosquitto_connect(AppCtx.mosq, AppCtx.settings["mqtt-broker"]["host"].as<std::string>().c_str(), AppCtx.settings["mqtt-broker"]["port"].as<unsigned short int>(), AppCtx.settings["mqtt-broker"]["conn-ttl"].as<int>());
    if (ret)
    {
        // std::cout << "Can't connect to Mosquitto broker" << std::endl;
        std::cout << "\x1b[31;1mCan't connect to Mosquitto broker at " << AppCtx.settings["mqtt-broker"]["host"].as<std::string>() << ":" << AppCtx.settings["mqtt-broker"]["port"].as<unsigned short int>() << "\x1b[0m" << std::endl;
        // return -1;
    } else {
        std::cout << "\x1b[32;1mConnected to Mosquitto broker\x1b[0m" << std::endl;
    }

    // Instantiate pipeline object
    pipeline = new g2f::Pipeline(loop, std::string(argv[1]), FLAGS_show_sink);

    // Sources creation
    g_print("Number of sources: %d\r\n", (int)sources_list.size());
    for (int i = 0; i < (int)sources_list.size(); i++)
    {
        pipeline->add_source(
            sources_list[i]["uri"].as<std::string>(),
            sources_list[i]["camera_id"].as<std::string>(),
            sources_list[i]["pad_index"].as<int>(),
            sources_list[i]["description"].as<std::string>(),
            use_nvurisrcbin);
    }

    // We add a message handler
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline->get_pipeline()));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop); // Look how to pass a function as argument
    gst_object_unref(bus);

    // Add probe to the nvanalytics src pad
    ProbeData_t *probe_data = new ProbeData_t;
    probe_data->appCtx = &AppCtx;
    probe_data->pipeline = pipeline;
    GstPad *nvanalytics_src_pad = gst_element_get_static_pad(pipeline->get_nvanalytics(), "src");
    if (!nvanalytics_src_pad)
    {
        g_print("Unable to get src pad of nvanalytics\n");
    }
    else
    {
        gst_pad_add_probe(nvanalytics_src_pad, GST_PAD_PROBE_TYPE_BUFFER, nvanalytics_src_pad_buffer_probe, (void*)probe_data, NULL);
    }
    gst_object_unref(nvanalytics_src_pad);

    // Create Thread for sending to mqtt broker
    std::thread broker_th = std::thread(send2MQTTBrokerTask, &AppCtx);


    g_timeout_add (1000, check_performance, (void*)pipeline);

    // Launch a thread to check the framecount of each camera
    g_print("Launching Stream Checker Thread...\n");
    AppCtx.stream_checker_thread = new std::thread(StreamCheckerTask, &AppCtx);


    // Change state to playing
    pipeline->run();

    // Wait till pipeline encounters an error or EOS
    g_print("Running...\n");
    g_main_loop_run(loop);

    // Out of the main loop, clean up nicely
    AppCtx.app_is_running = false;
    //
    g_print("Joinning Broker Thread...");
    AppCtx.cv.notify_all();
    broker_th.join();
    //
    if (AppCtx.mosq != 0)
    {
        g_print("Disconnecting from Mosquitto broker\n");
        mosquitto_disconnect(AppCtx.mosq);
        mosquitto_destroy(AppCtx.mosq);
        mosquitto_lib_cleanup();
    }
    g_print("Stopping playback\n");
    pipeline->stop();
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
    return 0;
}


static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;

    GST_CAT_DEBUG(NVDS_APP,
                  "Received message on bus: source %s, msg_type %s",
                  GST_MESSAGE_SRC_NAME(msg), GST_MESSAGE_TYPE_NAME(msg));

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_WARNING:
        {
            gchar *debug = NULL;
            GError *error = NULL;
            gst_message_parse_warning(msg, &error, &debug);
            g_printerr("WARNING from element %s: %s\n",
                    GST_OBJECT_NAME(msg->src), error->message);
            if (debug)
            {
                g_printerr("Debug info: %s\n", debug);
            }
            g_free(debug);
            g_error_free(error);
            break;
        }
        case GST_MESSAGE_ERROR:
        {
            gchar *debug;
            GError *error;
            // Expected messages
            const gchar *attempts_error = "Reconnection attempts exceeded for all sources or EOS received";

            gst_message_parse_error(msg, &error, &debug);
            g_printerr("ERROR from element %s: %s\n",
                    GST_OBJECT_NAME(msg->src), error->message);
            if (debug)
                g_printerr("Error details: %s\n", debug);
            g_free(debug);
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_ELEMENT:
        {
            if (gst_nvmessage_is_stream_eos(msg))
            {
                guint stream_id;
                if (gst_nvmessage_parse_stream_eos(msg, &stream_id))
                {
                    g_print("Got EOS from stream %d\n", stream_id);
                }
            }
            break;
        }
        default:
            break;
    }

    // we want to be notified again the next time there is a message
    // on the bus, so returning TRUE (FALSE means we want to stop watching
    // for messages on the bus and our callback should not be called again)
    return TRUE;
}

static GstPadProbeReturn
nvanalytics_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *)info->data;
    NvDsMetaList *l_frame = NULL;
    NvDsObjectMeta *obj_meta = NULL;
    NvDsDisplayMeta *display_meta = NULL;
    NvDsMetaList *l_obj = NULL;
    //
    NvDsClassifierMetaList *l_classifier_meta = NULL;
    NvDsClassifierMeta *classifier_meta = NULL;
    NvDsLabelInfoList *l_label_info = NULL;
    NvDsLabelInfo *label_info = NULL;
    //
    NvDsUserMetaList *l_user_meta = NULL;
    NvDsUserMeta *user_meta = NULL;
    //
    NvOSD_RectParams *bbox_params = NULL;
    //
    // Get timestamp
    std::chrono::time_point now = std::chrono::system_clock::now();
    //
    guint frame_number = 0;
    guint num_rects = 0;
    gboolean process_frame = false;
    // Probe Data
    ProbeData_t *probe_data = (ProbeData_t *)u_data;
    // App Context
    // AppCtx_t *AppCtx = (AppCtx_t *)u_data;
    AppCtx_t *AppCtx = probe_data->appCtx;
    // Pipeline
    g2f::Pipeline *pipeline = probe_data->pipeline;
    int frame_interval = AppCtx->settings["mqtt-broker"]["frame-interval"].as<int>();

    // Get batch metadata
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta)
    {
        // No batch meta attached.
        return GST_PAD_PROBE_OK;
    }

    // ----------------------------------------------------------------------
    // Find batch reid tensor in batch user meta. This part gets total values
    NvDsReidTensorBatch *pReidTensor = NULL;
    for (NvDsUserMetaList *l_batch_user = batch_meta->batch_user_meta_list; l_batch_user != NULL;
         l_batch_user = l_batch_user->next)
    {
        NvDsUserMeta *user_meta = (NvDsUserMeta *)l_batch_user->data;
        if (user_meta && user_meta->base_meta.meta_type == NVDS_TRACKER_BATCH_REID_META)
        {
            pReidTensor = (NvDsReidTensorBatch *)(user_meta->user_meta_data);
        }
    }
    // ----------------------------------------------------------------------

    // This for loop is for each frame in batch
    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next)
    {
        // Cast list element to NvDsFrameMeta
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        if (frame_meta == NULL)
        {
            // Ignore Null frame meta.
            continue;
        }

        // ---------------------------------------------------
        // Get Frame
        cv::Mat sourceFrame;
        // Buffer to store frame
        NvBufSurface *n_frame;
        //
        GstMapInfo in_map_info;
        memset(&in_map_info, 0, sizeof(in_map_info));

        if (gst_buffer_map(buf, &in_map_info, GST_MAP_READ))
        {
            n_frame = (NvBufSurface *)in_map_info.data;

            NvBufSurfaceMap(n_frame, frame_meta->batch_id, -1, NVBUF_MAP_READ);
            NvBufSurfaceSyncForCpu(n_frame, frame_meta->batch_id, -1);

            gint frame_width = (gint)n_frame->surfaceList[frame_meta->batch_id].width;
            gint frame_height = (gint)n_frame->surfaceList[frame_meta->batch_id].height;
            void *frame_data = n_frame->surfaceList[frame_meta->batch_id].mappedAddr.addr[0];
            size_t frame_step = n_frame->surfaceList[frame_meta->batch_id].pitch;

            try
            {
                sourceFrame = cv::Mat(frame_height, frame_width, CV_8UC4, frame_data, frame_step);

                if (AppCtx->save_frames)
                {
                    boost::asio::post(*AppCtx->pool, std::bind(saveFrameTask,
                                                                sourceFrame,
                                                                AppCtx->settings["output-directory"].as<std::string>(),
                                                                frame_meta->pad_index,
                                                                frame_meta->frame_num));
                }
            }
            catch (const std::exception &ex)
            {
                g_printerr("Caught exception: %s\n", ex.what());
            }
            catch (...)
            {
                g_printerr("Error processing frame %d from source %d\r\n", frame_meta->frame_num, frame_meta->source_id);
            }
        }
        gst_buffer_unmap(buf, &in_map_info);
        // ---------------------------------------------------

        // Get frame number
        frame_number = frame_meta->frame_num;
        // Get number of detected objects
        num_rects = frame_meta->num_obj_meta;
        // Reset frame processing flag
        process_frame = false;


        // Count frame
        pipeline->streams_[frame_meta->pad_index].frameCount++;


        // Send to mqtt broker
        // ===================
        if (frame_number % frame_interval == 0)
        {
            // Find camera info given pad_index
            std::string camera_id = "";
            std::string description = "";
            for (int i = 0; i < AppCtx->sources.size(); i++)
            {
                if (AppCtx->sources[i]["pad_index"].as<int>() == frame_meta->pad_index)
                {
                    camera_id = AppCtx->sources[i]["camera_id"].as<std::string>();
                    description = AppCtx->sources[i]["description"].as<std::string>();
                    break;
                }
            }
            g2f::FrameInfo frame_info(
                now.time_since_epoch().count(),
                camera_id,
                frame_meta->frame_num,
                sourceFrame
            );

            // // Save into FIFO queue
            // if (AppCtx->frame_list != nullptr) {
            //     std::unique_lock<std::mutex> lock(AppCtx->mtx);
            //     // Save
            //     AppCtx->frame_list->push(std::move(frame_info));
            //     // Notify 
            //     AppCtx->cv.notify_all();
            // }
            boost::asio::post(*AppCtx->pool, std::bind(send2redisTask,
                                                        AppCtx->redis,
                                                        AppCtx->redis_ttl,
                                                        frame_info));


        }

        // Loop over list of objects
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
        {
            // Cast list element to NvDsObjectMeta
            obj_meta = (NvDsObjectMeta *)(l_obj->data);
            if (obj_meta == NULL)
            {
                // Ignore Null object.
                continue;
            }

            // Get Bounding Box params
            bbox_params = &obj_meta->rect_params;
            // Roi list
            std::vector<std::string> roi_list;
            // Embedding length
            uint32_t numElements = 0;
            // Embeddings data
            float* embeddings_data = nullptr;

            // Loop over user meta data list
            for (l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL; l_user_meta = l_user_meta->next)
            {
                // Cast
                user_meta = (NvDsUserMeta *)(l_user_meta->data);

                if (frame_number % frame_interval == 0) {
                    // Extract object level meta data from NvDsAnalyticsObjInfo
                    if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS)
                    {
                        NvDsAnalyticsObjInfo *user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;
                        // if (user_meta_data->dirStatus.length() > 0)
                        // {
                        //     if (!(frame_number % AppCtx->settings["msgconv"]["frame-interval"].as<int>()))
                        //     {
                        //         g_print("\x1B[34;1mObject %ld (conf %0.2f) moving in direction: %s\x1B[0m\r\n", obj_meta->object_id, obj_meta->confidence, user_meta_data->dirStatus.c_str());
                        //         obj->direction = user_meta_data->dirStatus;
                        //         attach_detection_info = true;
                        //     }
                        // }
                        // for (const std::string &lc : user_meta_data->lcStatus)
                        // {
                        //     g_print("\x1B[34;1mFrame %d Object %ld (conf %0.2f) line crossing status: %s\x1B[0m\r\n", frame_number, obj_meta->object_id, obj_meta->confidence, lc.c_str());
                        //     obj->lineCrossing.emplace_back(lc);
                        //     attach_detection_info = true;
                        // }
                        // for (const std::string &oc : user_meta_data->ocStatus)
                        // {
                        //     g_print("\x1B[34;1mObject %ld (conf %0.2f) overcrowding status: %s\x1B[0m\r\n", obj_meta->object_id, obj_meta->confidence, oc.c_str());
                        // }
                        for (const std::string &roi : user_meta_data->roiStatus)
                        {
                            roi_list.emplace_back(roi);
                            // attach_detection_info = true;
                        }
                    }

                    // Find tracker reid tensor in object user meta.
                    if (user_meta->base_meta.meta_type == NVDS_TRACKER_OBJ_REID_META)
                    {
                        // Use embedding from tracker reid
                        gint reidInd = *((int32_t *)(user_meta->user_meta_data));
                        if (pReidTensor && pReidTensor->numFilled > 0 && reidInd >= 0)
                        {
                            // Get embedding length
                            numElements = pReidTensor->featureSize;
                            // Get embeddings data
                            embeddings_data = (float *)(pReidTensor->ptr_host + reidInd * pReidTensor->featureSize);
                            // // Get timestamp
                            // std::chrono::time_point now = std::chrono::system_clock::now();

                            // // Find camera info given pad_index
                            // std::string camera_id = "";
                            // std::string description = "";
                            // for (int i = 0; i < AppCtx->sources.size(); i++)
                            // {
                            //     if (AppCtx->sources[i]["pad_index"].as<int>() == frame_meta->pad_index)
                            //     {
                            //         camera_id = AppCtx->sources[i]["camera_id"].as<std::string>();
                            //         description = AppCtx->sources[i]["description"].as<std::string>();
                            //         break;
                            //     }
                            // }

                            // // Create Object
                            // g2f::ObjInfo obj_info(
                            //     camera_id, description,
                            //     frame_meta->pad_index, frame_meta->frame_num, obj_meta->object_id, obj_meta->confidence,
                            //     (int)obj_meta->rect_params.top, (int)obj_meta->rect_params.left, (int)obj_meta->rect_params.width, (int)obj_meta->rect_params.height,
                            //     numElements, embeddings_data, roi_list, now.time_since_epoch().count()
                            // );

                            // // Save into FIFO queue
                            // if (AppCtx->obj_list != nullptr) {
                            //     std::unique_lock<std::mutex> lock(AppCtx->mtx);
                            //     // Save
                            //     AppCtx->obj_list->push(std::move(obj_info));
                            //     // Notify 
                            //     AppCtx->cv.notify_all();
                            // }
                        }
                    }

                    // Save into FIFO queue
                    // ====================
                    // Find camera info given pad_index
                    std::string camera_id = "";
                    std::string description = "";
                    for (int i = 0; i < AppCtx->sources.size(); i++)
                    {
                        if (AppCtx->sources[i]["pad_index"].as<int>() == frame_meta->pad_index)
                        {
                            camera_id = AppCtx->sources[i]["camera_id"].as<std::string>();
                            description = AppCtx->sources[i]["description"].as<std::string>();
                            break;
                        }
                    }
                    // Create Object
                    g2f::ObjInfo obj_info(
                        camera_id, description,
                        frame_meta->pad_index, frame_meta->frame_num, obj_meta->class_id, obj_meta->object_id, obj_meta->confidence,
                        (int)obj_meta->rect_params.top, (int)obj_meta->rect_params.left, (int)obj_meta->rect_params.width, (int)obj_meta->rect_params.height,
                        numElements, embeddings_data, roi_list, now.time_since_epoch().count()
                    );
                    // Save into FIFO queue
                    if (AppCtx->obj_list != nullptr) {
                        std::unique_lock<std::mutex> lock(AppCtx->mtx);
                        // Save
                        AppCtx->obj_list->push(std::move(obj_info));
                        // Notify 
                        AppCtx->cv.notify_all();
                    }
                    // ====================
                }
            }
        }
    }
    return GST_PAD_PROBE_OK;
}

gchar* set_mqtt_message(const g2f::ObjInfo& obj_info) {
    JsonNode *rootNode;
    JsonObject *jobject;
    JsonArray *jArray, *jRoisArray;
    gchar *message = NULL;

    // Create JSON Object
    jobject = json_object_new();
    json_object_set_string_member(jobject, "version", "1.0");
    json_object_set_string_member(jobject, "type", "global_reid");
    json_object_set_int_member(jobject, "@timestamp", obj_info.ts_nanoseconds_);
    json_object_set_string_member(jobject, "camera_id", obj_info.camera_id_.c_str());
    json_object_set_int_member(jobject, "pad_index", obj_info.muxer_pad_index_);
    json_object_set_string_member(jobject, "description", obj_info.description_.c_str());
    json_object_set_int_member(jobject, "class_id", obj_info.class_id_);
    json_object_set_int_member(jobject, "object_id", obj_info.object_id_);
    json_object_set_int_member(jobject, "frame_num", obj_info.frame_num_);
    json_object_set_double_member(jobject, "confidence", obj_info.confidence_);
    json_object_set_int_member(jobject, "bbox_top", obj_info.bbox_top_); 
    json_object_set_int_member(jobject, "bbox_left", obj_info.bbox_left_);
    json_object_set_int_member(jobject, "bbox_width", obj_info.bbox_width_);
    json_object_set_int_member(jobject, "bbox_height", obj_info.bbox_height_);
    // Embeddings
    jArray = json_array_new();
    if (obj_info.embeddings_ != nullptr) {
        for (int i = 0; i < obj_info.embeddings_size_; i++) {
            json_array_add_double_element(jArray, obj_info.embeddings_[i]);
        }
    }
    json_object_set_array_member(jobject, "embeddings", jArray);
    // ROIs
    jRoisArray = json_array_new();
    for (const std::string &roi : obj_info.roi_) {
        json_array_add_string_element(jRoisArray, roi.c_str());
    }
    json_object_set_array_member(jobject, "roi", jRoisArray);
    // //
    // // Mat Frame
    // cv::Mat frame = obj_info.frame_;
    // if (!frame.empty()) {
    //     // Convert to JPEG
    //     std::vector<uchar> buf;
    //     std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 90};
    //     cv::imencode(".jpg", frame, buf, params);
    //     // Convert buf variable to a string_view object
    //     std::string_view plaintext_out(reinterpret_cast<const char*>(buf.data()), buf.size());
    //     // Convert to base64
    //     std::string encoded_str = base64::to_base64(plaintext_out);
    //     // Add to JSON
    //     json_object_set_string_member(jobject, "frame", encoded_str.c_str());
    // }

    // Create JSON root node
    rootNode = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(rootNode, jobject);
    // Convert JSON root node to string
    message = json_to_string(rootNode, true);
    json_node_free(rootNode);
    json_object_unref(jobject);
    // // Release JSON Array
    // if (jArray != NULL) {
    //     json_array_unref(jArray);
    // }
    // if (jRoisArray != NULL) {
    //     json_array_unref(jRoisArray);
    // }

    return message;
}

gchar* set_mqtt_frame_message(const g2f::FrameInfo& frame_info) {
    JsonNode *rootNode;
    JsonObject *jobject;
    JsonArray *jArray, *jRoisArray;
    gchar *message = NULL;

    // Create JSON Object
    jobject = json_object_new();
    json_object_set_string_member(jobject, "version", "1.0");
    json_object_set_string_member(jobject, "type", "camera_frame");
    json_object_set_int_member(jobject, "@timestamp", frame_info.ts_nanoseconds_);
    json_object_set_string_member(jobject, "camera_id", frame_info.camera_id_.c_str());
    json_object_set_int_member(jobject, "frame_num", frame_info.frame_num_);
    // Mat Frame
    cv::Mat frame = frame_info.frame_;
    if (!frame.empty()) {
        // Convert to JPEG
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 90};
        cv::imencode(".jpg", frame, buf, params);
        // Convert buf variable to a string_view object
        std::string_view plaintext_out(reinterpret_cast<const char*>(buf.data()), buf.size());
        // Convert to base64
        std::string encoded_str = base64::to_base64(plaintext_out);
        // Add to JSON
        json_object_set_string_member(jobject, "frame", encoded_str.c_str());
    }

    // Create JSON root node
    rootNode = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(rootNode, jobject);
    // Convert JSON root node to string
    message = json_to_string(rootNode, true);
    json_node_free(rootNode);
    json_object_unref(jobject);

    return message;
}


bool send_mqtt_message(AppCtx_t* ctx, gchar* message, const std::string& topic) {
    bool sentOK = false;
    int tries = 5;

    while (tries > 0) {
        // Publish a message to a topic
        int ret = mosquitto_publish(ctx->mosq, NULL, 
                    topic.c_str(), 
                    strlen(message), message, 
                    ctx->settings["mqtt-broker"]["qos"].as<int>(), 
                    ctx->settings["mqtt-broker"]["retained"].as<bool>());
        switch (ret)
        {
            case MOSQ_ERR_SUCCESS:
                sentOK = true;
                tries = 0;
                break;
            case MOSQ_ERR_INVAL:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_INVAL): the input parameters were invalid." << std::endl;
                tries = 0;
                break;
            case MOSQ_ERR_NOMEM:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_NOMEM): an out of memory condition occurred." << std::endl;
                break;
            case MOSQ_ERR_NO_CONN:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_NO_CONN): the client isn't connected to the broker. Reconnecting." << std::endl;
                mosquitto_reconnect(ctx->mosq);
                break;
            case MOSQ_ERR_PROTOCOL:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_PROTOCOL): there is a protocol error communicating with the broker." << std::endl;
                break;
            case MOSQ_ERR_PAYLOAD_SIZE:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_PAYLOAD_SIZE): the payloadlen is too large. Discarding packet." << std::endl;
                tries = 0;
                break;
            case MOSQ_ERR_MALFORMED_UTF8:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_MALFORMED_UTF8): the topic is not valid UTF-8." << std::endl;
                tries = 0;
                break;
            case MOSQ_ERR_QOS_NOT_SUPPORTED:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_QOS_NOT_SUPPORTED): the QoS is greater than that supported by the broker." << std::endl;
                tries = 0;
                break;
            case MOSQ_ERR_OVERSIZE_PACKET:
                std::cout << "[send2MQTTBrokerTask] Can't publish to topic (MOSQ_ERR_OVERSIZE_PACKET): the resulting packet would be larger than supported by the broker." << std::endl;
                tries = 0;
                break;
        }
        // Retry
        tries--;
    }

    return sentOK;
}

void send2MQTTBrokerTask(AppCtx_t* AppCtx) {
    while(AppCtx->app_is_running) {
        std::unique_lock<std::mutex> lock(AppCtx->mtx);
        while (AppCtx->obj_list->empty() && AppCtx->frame_list->empty()) AppCtx->cv.wait(lock);
        //
        if (!AppCtx->obj_list->empty()) {
            // Get front element
            g2f::ObjInfo& obj_info = AppCtx->obj_list->front();
            // Prepare payload
            gchar* message = set_mqtt_message(obj_info);
            // Send
            if (send_mqtt_message(AppCtx, message, AppCtx->settings["mqtt-broker"]["topic"].as<std::string>())) {
                // Pop
                AppCtx->obj_list->pop();
                // Release message
                g_free(message);
            }
        }
        // //
        // if (!AppCtx->frame_list->empty()) {
        //     // Get front element
        //     g2f::FrameInfo& frame_info = AppCtx->frame_list->front();
        //     // Prepare payload
        //     gchar* message = set_mqtt_frame_message(frame_info);
        //     // Send
        //     if (send_mqtt_message(AppCtx, message, AppCtx->settings["mqtt-broker"]["frames-topic"].as<std::string>())) {
        //         // Pop
        //         AppCtx->frame_list->pop();
        //         // Release message
        //         g_free(message);
        //     }
        // }
    }
}


bool prepareOutputDirectory(std::string output_dir, int num_sources) {
    bool result{true};

    try
    {
        std::filesystem::path frames_dir = output_dir;
        std::filesystem::remove_all(frames_dir);

        for (int i = 0; i < num_sources; i++)
        {
            if (i < 10)
                frames_dir = output_dir+"/source0" + std::to_string(i);
            else
                frames_dir = output_dir+"/source" + std::to_string(i);
            std::filesystem::create_directories(frames_dir);
        }
    }
    catch (...)
    {
        std::cout << "Exception preparing output frames dir." << std::endl;
        result = false;
    }

    return result;
}

void saveFrameTask(const cv::Mat& frame, const std::string& dir, gint pad_index, gint frame_num)
{
    try
    {
        cv::Mat frame_copy;
        cv::cvtColor(frame, frame_copy, cv::COLOR_BGRA2RGBA);

        gchar filename[1024];
        g_snprintf(filename, 1024, "%s/source%02d/frame_%07d.jpg",
                   dir.c_str(), pad_index, frame_num);

        bool result = cv::imwrite(filename, frame_copy);
    }
    catch (...)
    {
        g_printerr("saveFrameTask exception");
    }
}


void send2redisTask(sw::redis::Redis *redis, long long int ttl, const g2f::FrameInfo& frame_info)
{
    try
    {
        cv::Mat frame_copy; // (frame_info.frame_);
        cv::cvtColor(frame_info.frame_, frame_copy, cv::COLOR_BGRA2RGBA);
        // 1. Hbltar 

        gchar redis_key[1024];
        g_snprintf(redis_key, 1024, "%s_%d", frame_info.camera_id_.c_str(), frame_info.frame_num_);
        // std::cout << redis_key << std::endl;

        std::string_view sv(redis_key);
        sw::redis::StringView key(sv.data(), sv.size());
        //
        // Encode image as JPEG
        std::vector<uchar> buf(1048576);
        cv::imencode(".jpg", frame_copy, buf);
        // //
        // // Encode JPEG buffer as Base64
        // string img1_base64 = std::string::base64_encode(buf.data(), buf.size());
        //
        // Convert to Redis String
        std::string image_value(buf.begin(), buf.end());
        sw::redis::StringView value(image_value);

        redis->setex(key, ttl, value);
    }
    catch (const sw::redis::Error &e)
    {
        g_printerr("[send2redisTask] Redis Error: %s\n", e.what());
    }
    catch (const std::exception &e)
    {
        g_printerr("[send2redisTask] Exception: %s\n", e.what());
    }
    catch (...)
    {
        g_printerr("[send2redisTask] Unknown exception occurred\n");
    }    
}
