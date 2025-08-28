#pragma once

#include <queue> 
#include <thread>
#include <mutex>
#include <condition_variable>
#include <yaml-cpp/yaml.h>
#include <mosquitto.h>
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/thread/thread.hpp>
#include <opencv2/opencv.hpp>
#include "ObjInfo.hpp"

#include <sw/redis++/redis++.h>

/**
 * @brief Application Context
 * 
 */
typedef struct {
    // Configs
    YAML::Node settings;
    // Sources
    YAML::Node sources;
    // Reference to Threads Pool
    boost::asio::thread_pool* pool;
    // Reference to Mosquitto Client
    struct mosquitto* mosq;
    // ObjInfo FIFO Queue Pointer
    std::queue<g2f::ObjInfo>* obj_list;
    // FrameInfo FIFO Queue Pointer
    std::queue<g2f::FrameInfo>* frame_list;
    // Mutex variable for thread synchronization
    std::mutex mtx;
    // Condition variable for thread synchronization
    std::condition_variable cv;
    // Flag to indicate whether the application is running
    bool app_is_running;
    // Flag to indicate that the frames must be saved
    bool save_frames;
    // Reference to Redis object
    sw::redis::Redis *redis;
    // Time to live in seconds for data persisted in Redis
    long long int redis_ttl;
    // Mutex for streams
    GMutex lock_streams_info;
    // Checker Thread
    std::thread *stream_checker_thread;
} AppCtx_t;

/**
 * @brief Callback function for handling messages on the GStreamer bus.
 *
 * This function is called when a message is received on the GStreamer bus.
 * It handles different types of messages, such as warnings, errors, and element-specific messages.
 * For warnings and errors, it prints the corresponding message and debug information.
 * For element-specific messages, it checks if the message indicates the end of a stream.
 *
 * @param bus The GStreamer bus.
 * @param msg The GStreamer message.
 * @param data User data (in this case, a GMainLoop pointer).
 * @return gboolean TRUE to continue watching for messages on the bus, FALSE to stop watching.
 */
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);

/**
 * @brief Callback function for handling buffer probes on the source pad of the analytics element.
 *
 * This function is called when a buffer probe is triggered on the source pad of the analytics element.
 * It processes the buffer and extracts information such as frame metadata, object metadata, and user metadata.
 * It also handles the tracker reid tensor and performs operations based on the extracted data.
 *
 * @param pad The source pad of the analytics element.
 * @param info The buffer probe information.
 * @param u_data User data (in this case, a pointer to the application context).
 * @return GstPadProbeReturn The return value indicating the action to be taken by the probe.
 */
static GstPadProbeReturn
nvanalytics_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);


/**
 * @brief Creates an MQTT message in JSON format based on the provided `ObjInfo` object.
 *
 * This function takes an `ObjInfo` object and creates an MQTT message in JSON format.
 * The message includes various attributes such as version, timestamp, camera ID, object ID, frame number, confidence, bounding box coordinates, and embeddings.
 *
 * @param obj_info The `ObjInfo` object containing the information to be included in the MQTT message.
 * @return gchar* The MQTT message in JSON format.
 */
gchar* set_mqtt_message(const g2f::ObjInfo& obj_info);

/**
 * @brief Sends an MQTT message to the MQTT broker.
 *
 * This function sends an MQTT message to the MQTT broker using the provided application context and message.
 * It handles various error conditions such as invalid input parameters, out of memory conditions, protocol errors, and connection issues.
 *
 * @param ctx The application context containing the MQTT client and settings.
 * @param message The MQTT message to be sent.
 * @return bool TRUE if the message was sent successfully, FALSE otherwise.
 */
bool send_mqtt_message(AppCtx_t* ctx, gchar* message);

/**
 * @brief Sends objects to the MQTT broker.
 *
 * This function is a task that continuously sends objects to the MQTT broker.
 * It retrieves objects from the object list in the application context and sends them as MQTT messages.
 * The task waits for objects to be available in the object list using a condition variable.
 * Once an object is sent, it is removed from the object list.
 *
 * @param AppCtx The application context containing the object list and other settings.
 */
void send2MQTTBrokerTask(AppCtx_t* AppCtx);

/**
 * @brief Prepares the output directory for saving frames.
 *
 * This function prepares the output directory for saving frames.
 * It creates the directory if it does not exist and clears any existing files in the directory.
 *
 * @param output_dir The path to the output directory.
 * @param num_sources The number of camera sources.
 * @return bool TRUE if the output directory was prepared successfully, FALSE otherwise.
 */
bool prepareOutputDirectory(std::string output_dir, int num_sources);

/**
 * @brief Save the given frame to disk.
 *
 * @param frame Frame to save.
 * @param dir Directory to save the frame.
 * @param pad_index Muxer's Pad Index of the frame.
 * @param frame_num Frame number of the frame.
 */
void saveFrameTask(const cv::Mat& frame,const std::string& dir, gint pad_index, gint frame_num);

/**
 * @brief Send the given frame to the given Redis instance.
 *
 * @param redis Redis instance to send the frame to.
 * @param ttl Time to live of data in redis in seconds.
 * @param frame_info Frame Info
 */
void send2redisTask(sw::redis::Redis *redis, long long int ttl, const g2f::FrameInfo& frame_info);
