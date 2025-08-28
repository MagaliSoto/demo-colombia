#pragma once
#include <gst/gst.h>
#include <yaml-cpp/yaml.h>
#include "deepstream_config.h"
// RTSP Server
#include <gst/rtsp-server/rtsp-server.h>

namespace g2f 
{

typedef enum
{
  ERROR = -1,
  INIT = 0,
  RUNNING,
  STOPPED,
} PipelineStates;

typedef enum {
  NONE = 0,
  CHANGE_STATE_TO_NULL,
  CHECK_FOR_STATE_NULL,
  INITIALITING,     // Initial state, add to streammux, etc
  READY,            // Check that everything went well and return state to NONE
  //
  DELETE_STREAM,    // Delete stream from streammux
  WAITING,          // Waiting for the stream to be deleted
  ADD_STREAM,       // Add stream to streammux
} StreamResetStates;

/**
 * @brief Structure to store the information of the stream
 */
typedef struct
{
  // Source URI
  std::string uri;
  // Custom Camera ID given by the user
  std::string camera_id;
  // Muxer pad index
  int pad_index;
  // Source Description
  std::string description;
  // Frame counter
  int frameCount;
  // Seconds with no frames
  int noFrameCount;
  // Source bin
  GstElement *source_bin;
  // Reset enumeration states
  StreamResetStates reset;
} StreamSourceInfo;


class ChildAddedData
{
public:
  ChildAddedData(GstElement *source_bin, const YAML::Node &settings)
      : source_bin(source_bin), settings(settings) {}
  ~ChildAddedData() {}
  
  // Bin
  GstElement *source_bin;
  // Settings
  const YAML::Node &settings;
};

/**
    * @brief Create a source bin.
    *
    * This member function creates a source bin.
    *
    * @param index The index of the source.
    * @param uri The URI of the source.
    * @param use_nvurisrcbin A boolean flag indicating whether to use nvurisrcbin.
    * @param nvurisrcbin_settings The settings for nvurisrcbin.
    * @return A pointer to the source bin.
*/
GstElement *create_source_bin(int index, std::string uri, bool use_nvurisrcbin, const YAML::Node &settings);
GstElement *create_rtspsrc_bin(int index, std::string uri, const YAML::Node &settings);
void cb_newpad(GstElement *decodebin, GstPad *decoder_src_pad, gpointer data);
void decodebin_child_added(GstChildProxy *child_proxy, GObject *object, gchar *name, gpointer user_data);

void cb_newpad1(GstElement *rtspsrc, GstPad *decoder_src_pad, gpointer data);

void on_pad_added(GstElement *rtspsrc, GstPad *new_pad, gpointer gst_element);
void pad_added_handler (GstElement *src, GstPad *new_pad, GstElement *source_bin);


/**
 * @brief Connect a src pad to a sink pad.
 *
 * @param src Source element to get the pad from, with the name "src_%u".
 * @param sink Sink element to get the pad from, with the name "sink".
 * @param pad_index Index of the pad to get from the source element. if -1, then "src_%u" is used.
 * @return true if the pads were successfully linked.
 */
bool connect_src_sink_pads(GstElement *src, GstElement *sink, gint pad_index = -1);

/**
 * @brief Connect a src pad to a sink pad.
 *
 * @param muxer Muxer element to get the pad from, with the name "sink_%u".
 * @param src Source element to get the pad from, with the name "src".
 * @param pad_index Index of the pad to get from the muxer element. if -1, then "sink_%u" is used.
 * @return true if the pads were successfully linked.
 */
bool connect_muxer_src_pads(GstElement *muxer, GstElement *src, gint pad_index = -1);


class Pipeline {
public:
    Pipeline(GMainLoop *loop=nullptr, std::string config_file="", bool show_sink=false);
    ~Pipeline();
    void run();
    void stop();

    /**
    * @brief Add a source to the pipeline.
    *
    * This member function adds a source to the pipeline.
    *
    * @param index The index of the source.
    * @param uri The URI of the source.
    * @param camera_id The ID of the camera.
    * @param pad_index The index of the pad.
    * @param description The description of the source.
    * @param use_nvurisrcbin A boolean flag indicating whether to use nvurisrcbin.
    * @param readding A boolean flag indicating whether the source is being re-added.
    *                 If true, the source will not be counted.
    */
    void add_source(std::string uri, std::string camera_id, int pad_index, std::string description, bool use_nvurisrcbin, bool readding=false);

    /**
     * @brief Delete a source from the pipeline.
     *
     * This member function deletes a source from the pipeline.
     *
     * @param stream The StreamSourceInfo object containing the information of the source to be deleted.
     */
    void delete_source(g2f::StreamSourceInfo stream);

    /**
     * @brief Return the subjascent pipeline object.
    */
    GstElement* get_pipeline() { return pipeline_; }

    /**
     * @brief Return the nvdsanalytics element.
    */
    GstElement* get_nvanalytics() { return nvanalytics_; }

private:
    // Create pipeline object
    bool create_pipeline_object();
    // Create pipeline elements
    bool create_pipeline_elements();
    // Link pipeline elements
    bool link_pipeline_elements();
    // Create RTSP Server
    bool create_rtsp_server();
    bool create_rtsp_server(const YAML::Node &rtsp_config);


public:
    // Config file path
    std::string config_file_;
    // Parsed settings
    YAML::Node configs_;
    /**
     * @brief The main loop for the pipeline.
     *
     * This member variable represents the GMainLoop object used for controlling the main loop of the pipeline.
     * It is responsible for handling events and executing the pipeline's tasks.
     */
    GMainLoop*  loop_;
    //
    // Main pipeline element
    GstElement* pipeline_;
    //
    GstElement* streammux_;
    GstElement* preprocess_;
    GstElement* nvvidconv_;
    GstElement* capsfilter0_;
    GstElement* pgie_;
    GstElement* tracker_;
    GstElement* nvanalytics_;
    GstElement* tiler_;
    GstElement* nvosd_;
    GstElement* sinktee_;
    GstElement* sink_;
    // UDP Sink
    GstElement* nvvidconv_postosd_;
    GstElement* udp_caps_;
    GstElement* encoder_;
    GstElement* rtppay_;
    GstElement* udp_sink_;
    // TCP Sink
    GstElement* jpegenc_;
    GstElement* matroskamux_;
    GstElement* tcp_nvvidconv_postosd_;
    GstElement* tcp_caps_;
    GstElement* tcp_encoder_;
    GstElement* tcp_rtppay_;
    GstElement* tcp_server_sink_;
    // Queue elements
    GstElement* queue0_;
    GstElement* queue1_;
    GstElement* queue2_;
    GstElement* queue3_;
    GstElement* queue4_;
    GstElement* queue5_;
    GstElement* queue6_;
    GstElement* queue7_;
    GstElement* queue8_;
    // RTSP Server
    GstRTSPServer *server_;
    GstRTSPMountPoints *mounts_;
    GstRTSPMediaFactory *factory_;
    //
    NvDsGieType pgie_type_;
    //
    bool show_sink_;
    //
    // Number of streams
    int num_sources_;
    // Streams info
    StreamSourceInfo streams_[MAX_SOURCE_BINS];
};

}
