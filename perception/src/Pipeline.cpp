#include <stdexcept>
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
#include "Pipeline.hpp"

using namespace g2f;

// gst-launch-1.0 rtspsrc location=rtsp://admin:2Mini001.@10.93.27.201:554/h264/ch1/sub/av_stream latency=100 ! queue ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! videoscale ! video/x-raw,width=1280,height=720 ! autovideosink
GstElement *g2f::create_rtspsrc_bin(int index, std::string location, const YAML::Node &settings)
{
    // Bin
    GstElement *bin = NULL;
    // Source elements
    GstElement *rtspsrc = NULL, *queue = NULL, *rtph264depay = NULL;
    GstElement *h264parse = NULL, *avdec_h264 = NULL;
    GstElement *videoscale = NULL, *videorate = NULL, *videoconvert = NULL, *caps = NULL;
    // Bin name
    gchar bin_name[16] = {0,};

    g_snprintf(bin_name, 15, "rtspsrc-bin-%02d", index);

    g_print("Pipeline::create_source_bin(): Creating \x1b[33;1m%s\x1b[0m @ \x1b[33;1m%s\x1b[0m\r\n", bin_name, location.c_str());

    // Create a source GstBin to abstract this bin's content from the rest of the
    // pipeline
    bin = gst_bin_new(bin_name);

    // Source element for reading from the uri.
    rtspsrc = gst_element_factory_make("rtspsrc", "rtspsrc");
    queue = gst_element_factory_make("queue", "queue");
    rtph264depay = gst_element_factory_make("rtph264depay", "rtph264depay");
    h264parse = gst_element_factory_make("h264parse", "h264parse");
    avdec_h264 = gst_element_factory_make("avdec_h264", "avdec_h264");
    videoscale = gst_element_factory_make("videoscale", "videoscale");
    videorate = gst_element_factory_make("videorate", "videorate");
    videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    caps = gst_element_factory_make("capsfilter", "capsfilter");
    if (!bin || !rtspsrc || !queue || !rtph264depay || !h264parse || !avdec_h264 || !videoconvert || !videoscale || !videorate || !caps)
    {
        g_printerr("One element in source bin could not be created.\n");
        return NULL;
    }

    // Link elements
    gst_bin_add_many(GST_BIN(bin), rtspsrc, rtph264depay, h264parse, avdec_h264, videoscale, videorate, NULL);

    // Link elements within the bin
    if (!gst_element_link_many(rtph264depay, h264parse, avdec_h264, videoscale, videorate, NULL))
    {
        g_printerr("Elements in the bin could not be linked.\n");
        gst_object_unref(bin);
        return NULL;
    }

    // We set the input uri to the source element
    g_object_set(G_OBJECT(rtspsrc), "location", location.c_str(), NULL);
    g_object_set(G_OBJECT(rtspsrc), "latency", 100, NULL);
    g_object_set(G_OBJECT(rtspsrc), "is-live", 1, NULL);

    // // Set caps
    // GstCaps *caps_filter = gst_caps_new_simple("video/x-raw",
    //                                            "format", G_TYPE_STRING, "I420",
    //                                            "width", G_TYPE_INT, 1280,
    //                                            "height", G_TYPE_INT, 720,
    //                                            NULL);
    // g_object_set(G_OBJECT(caps), "caps", caps_filter, NULL);
    // gst_caps_unref(caps_filter);

    // Connect to the "pad-added" signal of the rtspsrc which generates a
    // callback once a new pad for raw data has beed created by the rtspsrc
    // g_signal_connect(G_OBJECT(rtspsrc), "pad-added",
    //                  G_CALLBACK((g2f::cb_newpad1)), bin);
    // g_signal_connect(G_OBJECT(rtspsrc), "pad-added",
    //                  G_CALLBACK((g2f::on_pad_added)), bin);
    g_signal_connect(G_OBJECT(rtspsrc), "pad-added",
                     G_CALLBACK((g2f::pad_added_handler)), bin);

    // g_signal_connect(G_OBJECT(rtspsrc), "child-added",
    //                  G_CALLBACK(g2f::decodebin_child_added), bin);


    // We need to create a ghost pad for the source bin which will act as a proxy
    // for the video decoder src pad. The ghost pad will not have a target right
    // now. Once the decode bin creates the video decoder and generates the
    // cb_newpad callback, we will set the ghost pad target to the video decoder
    // src pad.
    if (!gst_element_add_pad(bin, gst_ghost_pad_new_no_target("src", GST_PAD_SRC)))
    {
        g_printerr("Failed to add ghost pad in source bin\n");
        return NULL;
    }

    return bin;
}

GstElement *g2f::create_source_bin(int index, std::string uri, bool use_nvurisrcbin, const YAML::Node &settings)
{
    GstElement *bin = NULL, *uri_decode_bin = NULL;
    gchar bin_name[16] = {
        0,
    };

    g_snprintf(bin_name, 15, "source-bin-%02d", index);

    g_print("Pipeline::create_source_bin(): Creating %s @ %s\r\n", bin_name, uri.c_str());

    // Create a source GstBin to abstract this bin's content from the rest of the
    // pipeline
    bin = gst_bin_new(bin_name);

    // Source element for reading from the uri.
    // We will use decodebin and let it figure out the container format of the
    // stream and the codec and plug the appropriate demux and decode plugins.
    if (use_nvurisrcbin)
    {
        // https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvurisrcbin.html
        uri_decode_bin = gst_element_factory_make("nvurisrcbin", "uri-decode-bin");
        g_object_set(G_OBJECT(uri_decode_bin),
                     "async-handling", settings["nvurisrcbin"]["async-handling"].as<int>(),
                     "dec-skip-frames", settings["nvurisrcbin"]["dec-skip-frames"].as<int>(),
                     "cudadec-memtype", settings["nvurisrcbin"]["cudadec-memtype"].as<int>(),   
                     "num-extra-surfaces", settings["nvurisrcbin"]["num-extra-surfaces"].as<int>(), 
                     "gpu-id", settings["nvurisrcbin"]["gpu-id"].as<int>(),
                     "drop-frame-interval", settings["nvurisrcbin"]["drop-frame-interval"].as<int>(),
                     "select-rtp-protocol", settings["nvurisrcbin"]["select-rtp-protocol"].as<int>(),
                     "rtsp-reconnect-interval", settings["nvurisrcbin"]["rtsp-reconnect-interval"].as<int>(),
                     "rtsp-reconnect-attempts", settings["nvurisrcbin"]["rtsp-reconnect-attempts"].as<int>(),
                     "low-latency-mode", settings["nvurisrcbin"]["low-latency-mode"].as<int>(),
                     "source-id", index,
                     "latency", settings["nvurisrcbin"]["latency"].as<int>(),
                     "drop-on-latency", settings["nvurisrcbin"]["drop-on-latency"].as<int>(),
                     "udp-buffer-size", settings["nvurisrcbin"]["udp-buffer-size"].as<unsigned long int>(),
                     "type", settings["nvurisrcbin"]["type"].as<int>(),
                     NULL);
    }
    else
    {
        // https://gstreamer.freedesktop.org/documentation/rtsp/rtspsrc.html?gi-language=c
        // https://gstreamer.freedesktop.org/documentation/playback/uridecodebin.html?gi-language=c
        uri_decode_bin = gst_element_factory_make("uridecodebin", "uri-decode-bin");
    }

    if (!bin || !uri_decode_bin)
    {
        g_printerr("One element in source bin could not be created.\n");
        return NULL;
    }

    // We set the input uri to the source element
    g_object_set(G_OBJECT(uri_decode_bin), "uri", uri.c_str(), NULL);

    // Connect to the "pad-added" signal of the decodebin which generates a
    // callback once a new pad for raw data has beed created by the decodebin
    g_signal_connect(G_OBJECT(uri_decode_bin), "pad-added",
                     G_CALLBACK((g2f::cb_newpad)), bin);

    g2f::ChildAddedData *child_added_data = new g2f::ChildAddedData(bin, settings);
    g_signal_connect(G_OBJECT(uri_decode_bin), "child-added",
                     G_CALLBACK(g2f::decodebin_child_added), child_added_data);

    gst_bin_add(GST_BIN(bin), uri_decode_bin);

    // We need to create a ghost pad for the source bin which will act as a proxy
    // for the video decoder src pad. The ghost pad will not have a target right
    // now. Once the decode bin creates the video decoder and generates the
    // cb_newpad callback, we will set the ghost pad target to the video decoder
    // src pad.
    if (!gst_element_add_pad(bin, gst_ghost_pad_new_no_target("src", GST_PAD_SRC)))
    {
        g_printerr("Failed to add ghost pad in source bin\n");
        return NULL;
    }

    return bin;
}

void g2f::cb_newpad1(GstElement *rtspsrc, GstPad *new_pad, gpointer data)
{
    g_print("Pipeline::cb_newpad1(): Pad created: %s\n", GST_PAD_NAME(new_pad));

    GstElement *source_bin = (GstElement *)data;
    // Get queue sink pad from inside bin, getting queue element by name
    GstElement *queue = gst_bin_get_by_name(GST_BIN(source_bin), "queue");
    if (!queue)
    {
        g_printerr("Failed to get queue element from source bin\n");
        return;
    }
    
    GstPad *sink_pad = gst_element_get_static_pad(queue, "sink");
    if (gst_pad_is_linked(sink_pad))
    {
        g_print("Pad already linked. Ignoring.\n");
        gst_object_unref(sink_pad);
        return;
    }
    if (gst_pad_link(new_pad, sink_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Failed to link rtspsrc to queue.\n");
    }
    else
    {
        g_print("Linked rtspsrc to queue successfully.\n");
        // Get the source bin ghost pad
        GstPad *bin_ghost_pad = gst_element_get_static_pad(source_bin, "src");
        GstElement *caps = gst_bin_get_by_name(GST_BIN(source_bin), "capsfilter");
        // Get the caps src pad
        GstPad *caps_pad = gst_element_get_static_pad(caps, "src");
        // Link the ghost pad to the caps src pad
        if (!gst_ghost_pad_set_target(GST_GHOST_PAD(bin_ghost_pad), caps_pad))
        {
            g_printerr("Failed to link decoder src pad to source bin ghost pad\n");
        }
        gst_object_unref(bin_ghost_pad);
    }
    // Unref the sink pad
    gst_object_unref(sink_pad);


}


// Callback Function 1
void g2f::on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstElement *source_bin = (GstElement *)data;
    // Get queue sink pad from inside bin, getting queue element by name
    GstElement *queue = gst_bin_get_by_name(GST_BIN(source_bin), "rtph264depay");
    if (!queue)
    {
        g_printerr("Failed to get queue element from source bin\n");
        return;
    }
    //
    GstElement *videorate = gst_bin_get_by_name(GST_BIN(source_bin), "videorate");
    if (!videorate)
    {
        g_printerr("Failed to get videoconvert element from source bin\n");
        return;
    }

	// Link two Element with named pad
	GstPad *sink_pad = gst_element_get_static_pad (queue, "sink");
	if(gst_pad_is_linked(sink_pad)) {
		g_print("rtspsrc and depay are already linked. Ignoring\n");
		return;
	}
    gst_element_link_pads(element, gst_pad_get_name(pad), queue, "sink");

    // Get the source bin ghost pad
    GstPad *bin_ghost_pad = gst_element_get_static_pad(source_bin, "src");
	GstPad *sink_pad1 = gst_element_get_static_pad (videorate, "src");
	if(gst_pad_is_linked(sink_pad1)) {
		g_print("rtspsrc and depay are already linked. Ignoring\n");
		return;
	}
    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(bin_ghost_pad),
                                sink_pad1))
    {
        g_printerr("Failed to link decoder src pad to source bin ghost pad\n");
    }
    gst_object_unref(bin_ghost_pad);    
}


// Callback Function 2
void g2f::pad_added_handler (GstElement *src, GstPad *new_pad, GstElement *data)
{
    GstElement *source_bin = (GstElement *)data;
    // Get queue sink pad from inside bin, getting queue element by name
    GstElement *depay = gst_bin_get_by_name(GST_BIN(source_bin), "rtph264depay");
    if (!depay)
    {
        g_printerr("Failed to get depay element from source bin\n");
        return;
    }
    //
    GstElement *videorate = gst_bin_get_by_name(GST_BIN(source_bin), "videorate");
    if (!videorate)
    {
        g_printerr("Failed to get videoconvert element from source bin\n");
        return;
    }

    // More control of link two pad (comes from tutorial)
	GstPad *sink_pad = gst_element_get_static_pad (depay, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
    GstPad *bin_ghost_pad = NULL;
	GstPad *sink_pad1 = NULL;


	g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

	/* If our converter is already linked, we have nothing to do here */
	if (gst_pad_is_linked (sink_pad)) {
		g_print (" Sink pad from %s already linked. Ignoring.\n", GST_ELEMENT_NAME (src));
		goto exit;
	}

	/* Check the new pad's type */
	new_pad_caps = gst_pad_get_current_caps (new_pad);
	new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
	new_pad_type = gst_structure_get_name (new_pad_struct);
	/* Check the new pad's name */
	if (!g_str_has_prefix (new_pad_type, "application/x-rtp")) {
		g_print ("  It is not the right pad.  Need recv_rtp_src_. Ignoring.\n");
		goto exit;
	}

	/* Attempt the link */
	ret = gst_pad_link (new_pad, sink_pad);
	if (GST_PAD_LINK_FAILED (ret)) {
		g_print ("  Type is '%s' but link failed.\n", new_pad_type);
	} else {
		g_print ("  Link succeeded (type '%s').\n", new_pad_type);
	}

    // Get the source bin ghost pad
    bin_ghost_pad = gst_element_get_static_pad(source_bin, "src");
	sink_pad1 = gst_element_get_static_pad (videorate, "src");
	if(gst_pad_is_linked(sink_pad1)) {
		g_print("rtspsrc and depay are already linked. Ignoring\n");
		return;
	}
    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(bin_ghost_pad),
                                sink_pad1))
    {
        g_printerr("Failed to link decoder src pad to source bin ghost pad\n");
    }
    gst_object_unref(bin_ghost_pad);    

exit:
	/* Unreference the new pad's caps, if we got them */
	if (new_pad_caps != NULL)
		gst_caps_unref (new_pad_caps);

	/* Unreference the sink pad */
	gst_object_unref (sink_pad);
}


void g2f::cb_newpad(GstElement *decodebin, GstPad *decoder_src_pad, gpointer data)
{
    GstCaps *caps = gst_pad_get_current_caps(decoder_src_pad);
    if (!caps)
    {
        caps = gst_pad_query_caps(decoder_src_pad, NULL);
    }
    const GstStructure *str = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(str);
    GstElement *source_bin = (GstElement *)data;
    GstCapsFeatures *features = gst_caps_get_features(caps, 0);

    // Need to check if the pad created by the decodebin is for video and not
    // audio.
    if (!strncmp(name, "video", 5))
    {
        // Link the decodebin pad only if decodebin has picked nvidia
        // decoder plugin nvdec_*. We do this by checking if the pad caps contain
        // NVMM memory features.
        if (gst_caps_features_contains(features, "memory:NVMM"))
        {
            // Get the source bin ghost pad
            GstPad *bin_ghost_pad = gst_element_get_static_pad(source_bin, "src");
            if (!gst_ghost_pad_set_target(GST_GHOST_PAD(bin_ghost_pad),
                                          decoder_src_pad))
            {
                g_printerr("Failed to link decoder src pad to source bin ghost pad\n");
            }
            gst_object_unref(bin_ghost_pad);
        }
        else
        {
            g_printerr("Error: Decodebin did not pick nvidia decoder plugin.\n");
        }
    }
}

void g2f::decodebin_child_added(GstChildProxy *child_proxy, GObject *object,
                                gchar *name, gpointer user_data)
{
    g_print("Decodebin child added: %s\n", name);
    if (g_strrstr(name, "decodebin") == name)
    {
        g_signal_connect(G_OBJECT(object), "child-added",
                         G_CALLBACK(decodebin_child_added), user_data);
    }

    if (g_strrstr(name, "source") == name)
    {
        g2f::ChildAddedData *child_added_data = (g2f::ChildAddedData *)user_data;
        g_object_set(G_OBJECT(object), 
            "drop-on-latency", child_added_data->settings["uridecodebin"]["drop-on-latency"].as<int>(),
            "latency", child_added_data->settings["uridecodebin"]["latency"].as<int>(),
            "buffer-mode", child_added_data->settings["uridecodebin"]["buffer-mode"].as<int>(),
            "ntp-sync", child_added_data->settings["uridecodebin"]["ntp-sync"].as<int>(),
            "ntp-time-source", child_added_data->settings["uridecodebin"]["ntp-time-source"].as<int>(),
            "is-live", child_added_data->settings["uridecodebin"]["is-live"].as<int>(),
            "do-retransmission", child_added_data->settings["uridecodebin"]["do-retransmission"].as<int>(),
            "do-rtcp", child_added_data->settings["uridecodebin"]["do-rtcp"].as<int>(),
            "do-rtsp-keep-alive", child_added_data->settings["uridecodebin"]["do-rtsp-keep-alive"].as<int>(),
            "probation", child_added_data->settings["uridecodebin"]["probation"].as<int>(),
            "retry", child_added_data->settings["uridecodebin"]["retry"].as<int>(),
            "timeout", child_added_data->settings["uridecodebin"]["timeout"].as<unsigned long int>(),
            "udp-buffer-size", child_added_data->settings["uridecodebin"]["udp-buffer-size"].as<unsigned long int>(),
            "udp-reconnect", child_added_data->settings["uridecodebin"]["udp-reconnect"].as<int>(),
            "protocols", child_added_data->settings["uridecodebin"]["protocols"].as<int>(),
            "tcp-timeout", child_added_data->settings["uridecodebin"]["tcp-timeout"].as<unsigned long int>(),
            // "tcp-timestamp", child_added_data->settings["uridecodebin"]["tcp-timestamp"].as<int>(),
            "teardown-timeout", child_added_data->settings["uridecodebin"]["teardown-timeout"].as<unsigned long int>(),
            NULL);
    }
}

bool g2f::connect_src_sink_pads(GstElement *src, GstElement *sink, gint pad_index)
{
    GstPad *src_pad = NULL;
    //
    gchar pad_name[16] = {};
    g_snprintf(pad_name, 15, "src_%u", pad_index);
    //
    // Get src pads from Tee element
    if (pad_index >= 0)
        src_pad = gst_element_request_pad_simple(src, pad_name);
    else
        src_pad = gst_element_request_pad_simple(src, "src_%u");
    if (!src_pad)
    {
        g_printerr("Unable to get request source pads from element\n");
        return false;
    }
    // Link Tee src_%u pad to sink pad
    GstPad *sink_pad = gst_element_get_static_pad(sink, "sink");
    if (gst_pad_link(src_pad, sink_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Unable to link src and sink pads\n");
        gst_object_unref(sink_pad);
        return false;
    }
    gst_object_unref(sink_pad);
    gst_object_unref(src_pad);

    return true;
}

bool g2f::connect_muxer_src_pads(GstElement *muxer, GstElement *src, gint pad_index)
{
    GstPad *sink_pad = NULL;
    GstPad *src_pad = NULL;
    //
    // Get src pads from Tee element
    if (pad_index >= 0)
    {
        gchar pad_name[16] = {};
        g_snprintf(pad_name, 15, "sink_%u", pad_index);
        sink_pad = gst_element_request_pad_simple(muxer, pad_name);
    }
    else
        sink_pad = gst_element_request_pad_simple(muxer, "sink_%u");
    if (!sink_pad)
    {
        g_printerr("Unable to get request sink pads from muxer\n");
        return false;
    }
    //
    // Get source pad
    src_pad = gst_element_get_static_pad(src, "src");
    //
    // Link
    if (gst_pad_link(src_pad, sink_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Unable to link src and muxer pads\n");
        gst_object_unref(src_pad);
        gst_object_unref(sink_pad);
        return false;
    }
    gst_object_unref(sink_pad);
    gst_object_unref(src_pad);

    return true;
}


Pipeline::Pipeline(GMainLoop *loop, std::string config_file, bool show_sink) : loop_(loop), pipeline_(nullptr), config_file_(config_file), show_sink_(show_sink)
{
    // Initialize PGIE type
    char cConfig[4096];
    sprintf(cConfig, "%s", config_file_.c_str());
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_gie_type(&pgie_type_, cConfig, "primary-gie"))
    {
        throw std::runtime_error("Failed to parse GIE type.");
    }

    // Initialize number of sources
    num_sources_ = 0;
    // Load configuration file
    if (!config_file_.empty())
    {
        // Parse configuration file
        configs_ = YAML::LoadFile(config_file_);
    }

    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            create_rtsp_server(configs_["rtsp-sink"]["tcp-sink"]);
        }
        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            create_rtsp_server(configs_["rtsp-sink"]["udp-sink"]);
        }
        // // Create RTSP Server
        // if (!create_rtsp_server()) {
        //     throw std::runtime_error("Failed to create RTSP server.");
        // }
    }

    // Create Pipeline element that will form a connection of other elements
    if (!create_pipeline_object())
    {
        throw std::runtime_error("Failed to create pipeline object.");
    }
    g_print("Pipeline::Pipeline(): Pipeline object created.\n");

    // Create pipeline elements
    if (!create_pipeline_elements())
    {
        throw std::runtime_error("Failed to create pipeline elements.");
    }
    g_print("Pipeline::Pipeline(): Pipeline elements created.\n");

    // Set up the pipeline
    if (!link_pipeline_elements())
    {
        throw std::runtime_error("Elements could not be linked.");
    }
}

Pipeline::~Pipeline()
{
    // Free resources
    gst_object_unref(GST_OBJECT(pipeline_));
}

void Pipeline::run()
{
    /* Set the pipeline to "playing" state */
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    /* wait until it's up and running or failed */
    if (gst_element_get_state(pipeline_, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE)
    {
        g_error("Failed to go into PLAYING state");
    }
}

void Pipeline::stop()
{
    // Stop the pipeline
    gst_element_set_state(pipeline_, GST_STATE_NULL);
}

bool Pipeline::create_pipeline_object()
{
    // Create pipeline object
    pipeline_ = gst_pipeline_new("pipeline");
    if (!pipeline_)
    {
        g_printerr("Failed to create pipeline object.\n");
        return false;
    }
    return true;
}

bool Pipeline::create_pipeline_elements()
{
    // Queues
    queue0_ = gst_element_factory_make("queue", "queue0");
    queue1_ = gst_element_factory_make("queue", "queue1");
    queue2_ = gst_element_factory_make("queue", "queue2");
    queue3_ = gst_element_factory_make("queue", "queue3");
    queue4_ = gst_element_factory_make("queue", "queue4");
    queue5_ = gst_element_factory_make("queue", "queue5");
    queue6_ = gst_element_factory_make("queue", "queue6");
    queue7_ = gst_element_factory_make("queue", "queue7");
    queue8_ = gst_element_factory_make("queue", "queue8");

    // Create nvstreammux instance to form batches from one or more sources.
    streammux_ = gst_element_factory_make("nvstreammux", "stream-muxer");

    // Preprocess plugin
    preprocess_ = gst_element_factory_make("nvdspreprocess", "preprocess-plugin");

    // Use nvinfer or nvinferserver to run inferencing on decoder's output,
    // behaviour of inferencing is set through config file
    if (pgie_type_ == NVDS_GIE_PLUGIN_INFER_SERVER)
    {
        pgie_ = gst_element_factory_make("nvinferserver", "primary-nvinference-engine");
    }
    else
    {
        pgie_ = gst_element_factory_make("nvinfer", "primary-nvinference-engine");
    }

    // Tracker
    tracker_ = gst_element_factory_make("nvtracker", "tracker");

    // Analytics plugin
    nvanalytics_ = gst_element_factory_make("nvdsanalytics", "nvanalytics");

    // Use nvtiler to composite the batched frames into a 2D tiled array based
    // on the source of the frames.
    tiler_ = gst_element_factory_make("nvmultistreamtiler", "nvtiler");

    // Use convertor to convert from NV12 to RGBA as required by nvosd
    nvvidconv_ = gst_element_factory_make("nvvideoconvert", "nvvideo-converter");

    // Filter Capabilities
    capsfilter0_ = gst_element_factory_make("capsfilter", NULL);

    // Create OSD to draw on the converted RGBA buffer
    nvosd_ = gst_element_factory_make("nvdsosd", "nv-onscreendisplay");

    // Create tee to split the OSD Rendered buffer and the remote display through RTSP
    sinktee_ = gst_element_factory_make("tee", "sinks-tee");

    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            // TCP Sink
            // jpegenc_ = gst_element_factory_make("jpegenc", "jpegenc");
            // matroskamux_ = gst_element_factory_make("matroskamux", "matroskamux");

            tcp_nvvidconv_postosd_ = gst_element_factory_make("nvvideoconvert", "tcp_converter_postosd");
            tcp_caps_ = gst_element_factory_make("capsfilter", "filter");

            if (configs_["rtsp-sink"]["tcp-sink"]["rtsp-server"]["encoding-name"].as<std::string>() == "H264")
            {
                // H264
                tcp_encoder_ = gst_element_factory_make("nvv4l2h264enc", "encoder");
                tcp_rtppay_ = gst_element_factory_make("rtph264pay", "rtppay");
            }
            else
            {
                // H265
                tcp_encoder_ = gst_element_factory_make("nvv4l2h265enc", "encoder");
                tcp_rtppay_ = gst_element_factory_make("rtph265pay", "rtppay");
            }

            tcp_server_sink_ = gst_element_factory_make("tcpserversink", "tcp-server-sink");
        }

        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            // UDP Sink
            nvvidconv_postosd_ = gst_element_factory_make("nvvideoconvert", "converter_postosd");
            udp_caps_ = gst_element_factory_make("capsfilter", "filter");

            if (configs_["rtsp-sink"]["udp-sink"]["rtsp-server"]["encoding-name"].as<std::string>() == "H264")
            {
                // H264
                encoder_ = gst_element_factory_make("nvv4l2h264enc", "encoder");
                rtppay_ = gst_element_factory_make("rtph264pay", "rtppay");
            }
            else
            {
                // H265
                encoder_ = gst_element_factory_make("nvv4l2h265enc", "encoder");
                rtppay_ = gst_element_factory_make("rtph265pay", "rtppay");
            }

            udp_sink_ = gst_element_factory_make("udpsink", "udpsink");
        }
    }

    // Finally render the osd output
    if (show_sink_)
        sink_ = gst_element_factory_make("nveglglessink", "nvvideo-renderer");
    else
        sink_ = gst_element_factory_make("fakesink", "nvvideo-renderer");

    if (!streammux_ || !preprocess_ || !pgie_ || !tracker_ || !nvanalytics_ || !tiler_ || !nvvidconv_ || !capsfilter0_ || !nvosd_ || !sinktee_ || !sink_)
    {
        return false;
    }
    if (!queue0_ || !queue1_ || !queue2_ || !queue3_ || !queue4_ || !queue5_ || !queue6_ || !queue7_ || !queue8_)
    {
        return false;
    }
    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            // if (!jpegenc_ || !matroskamux_ || !tcp_server_sink_) {
            if (!tcp_nvvidconv_postosd_ || !tcp_caps_ || !tcp_encoder_ || !tcp_rtppay_ || !tcp_server_sink_)
            {
                g_print("Pipeline::create_pipeline_elements(): Failed to create TCP Sink elements.\n");
                return false;
            }
        }
        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            if (!nvvidconv_postosd_ || !udp_caps_ || !encoder_ || !rtppay_ || !udp_sink_)
            {
                g_print("Pipeline::create_pipeline_elements(): Failed to create UDP Sink elements.\n");
                return false;
            }
        }
    }

    // Set properties
    char config_file[4096];
    sprintf(config_file, "%s", config_file_.c_str());
    //
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_streammux(streammux_, config_file, "streammux"))
    {
        g_printerr("Error in parsing configuration file for streammux.\n");
        return false;
    }
    //
    g_object_set(G_OBJECT(preprocess_), "config-file", configs_["preprocess"]["config-file"].as<std::string>().c_str(), NULL);
    //
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_gie(pgie_, config_file, "primary-gie"))
    {
        g_printerr("Error in parsing configuration file for primary GIE.\n");
        return false;
    }
    //
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_tracker(tracker_, config_file, "tracker"))
    {
        g_printerr("Error in parsing configuration file for TRACKER.\n");
        return false;
    }
    //
    g_object_set(G_OBJECT(nvanalytics_), "config-file", configs_["nvanalytics"]["config-file"].as<std::string>().c_str(), NULL);
    //
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_tiler(tiler_, config_file, "tiler"))
    {
        g_printerr("Error in parsing configuration file for tiler.\n");
        return false;
    }
    //
    g_object_set(G_OBJECT(nvvidconv_), "nvbuf-memory-type", 3, NULL);
    //
    GstCaps *filtercaps = gst_caps_new_simple("video/x-raw(memory:NVMM)",
                                              "format", G_TYPE_STRING, "RGBA",
                                              NULL);
    g_object_set(G_OBJECT(capsfilter0_), "caps", filtercaps, NULL);
    gst_caps_unref(filtercaps);
    //
    if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_osd(nvosd_, config_file, "osd"))
    {
        g_printerr("Error in parsing configuration file for OSD.\n");
        return false;
    }
    //
    if (show_sink_)
    {
        if (NVDS_YAML_PARSER_SUCCESS != nvds_parse_egl_sink(sink_, config_file, "egl-sink"))
        {
            g_printerr("Error in parsing configuration file for EGL sink.\n");
            return false;
        }
    }
    //
    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            // Set properties for TCP Sink
            // g_object_set(G_OBJECT(jpegenc_), "quality", configs_["rtsp-sink"]["tcp-sink"]["jpegenc"]["quality"].as<int>(), NULL);

            // Set properties for UDP Sink
            auto filtercaps = gst_caps_new_simple("video/x-raw(memory:NVMM)", "format", G_TYPE_STRING, "I420", NULL);
            g_object_set(G_OBJECT(tcp_caps_), "caps", filtercaps, NULL);
            gst_caps_unref(filtercaps);
            //
            g_object_set(G_OBJECT(tcp_encoder_), "bitrate", configs_["rtsp-sink"]["tcp-sink"]["encoder"]["bitrate"].as<long long int>(), NULL);

            g_object_set(G_OBJECT(tcp_server_sink_),
                         "host", configs_["rtsp-sink"]["tcp-sink"]["sink"]["host"].as<std::string>().c_str(),
                         "port", configs_["rtsp-sink"]["tcp-sink"]["sink"]["port"].as<int>(),
                         "sync", configs_["rtsp-sink"]["tcp-sink"]["sink"]["sync"].as<int>(),
                         NULL);
        }

        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            // Set properties for UDP Sink
            auto filtercaps = gst_caps_new_simple("video/x-raw(memory:NVMM)", "format", G_TYPE_STRING, "I420", NULL);
            g_object_set(G_OBJECT(udp_caps_), "caps", filtercaps, NULL);
            gst_caps_unref(filtercaps);
            //
            g_object_set(G_OBJECT(encoder_), "bitrate", configs_["rtsp-sink"]["udp-sink"]["encoder"]["bitrate"].as<long long int>(), NULL);
            //
            g_object_set(G_OBJECT(udp_sink_),
                         "host", configs_["rtsp-sink"]["udp-sink"]["sink"]["host"].as<std::string>().c_str(),
                         "port", configs_["rtsp-sink"]["udp-sink"]["sink"]["port"].as<int>(),
                         "async", configs_["rtsp-sink"]["udp-sink"]["sink"]["async"].as<int>(),
                         "sync", configs_["rtsp-sink"]["udp-sink"]["sink"]["sync"].as<int>(),
                         NULL);
        }
    }
    //
    return true;
}

bool Pipeline::link_pipeline_elements()
{
    // Set up the pipeline
    gst_bin_add_many(GST_BIN(pipeline_), queue0_, queue1_, queue2_, queue3_, queue4_, queue5_, queue6_, NULL);
    gst_bin_add_many(GST_BIN(pipeline_), streammux_, preprocess_, pgie_, tracker_, nvanalytics_, NULL);
    gst_bin_add_many(GST_BIN(pipeline_), tiler_, nvvidconv_, capsfilter0_, nvosd_, sinktee_, sink_, NULL);
    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            // gst_bin_add_many(GST_BIN(pipeline_), queue8_, jpegenc_, matroskamux_, tcp_server_sink_, NULL);
            gst_bin_add_many(GST_BIN(pipeline_), queue8_, tcp_nvvidconv_postosd_, tcp_caps_, tcp_encoder_, tcp_rtppay_, tcp_server_sink_, NULL);
        }
        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            gst_bin_add_many(GST_BIN(pipeline_), queue7_, nvvidconv_postosd_, udp_caps_, encoder_, rtppay_, udp_sink_, NULL);
        }
    }
    //
    // Connect elements
    if (gst_element_link_many(streammux_, queue0_, nvvidconv_, capsfilter0_, pgie_, queue1_, tracker_, queue2_, nvanalytics_, queue3_, tiler_, queue4_, nvosd_, queue5_, sinktee_, NULL) != TRUE)
    {
        return false;
    }
    //
    // Connect sinktee to OSD Render
    if (gst_element_link_many(queue6_, sink_, NULL) != TRUE)
    {
        return false;
    }
    if (!connect_src_sink_pads(sinktee_, queue6_))
    {
        return false;
    }
    //
    // Connect RSTP Sink if enabled
    if (configs_["rtsp-sink"]["enable"].as<int>())
    {
        if (configs_["rtsp-sink"]["tcp-sink"]["enable"].as<int>())
        {
            g_print("Pipeline::link_pipeline_elements(): TCP Sink enabled.\n");
            // Connect sinktee to TCP Sink
            // if (gst_element_link_many(queue8_, jpegenc_, tcp_server_sink_, NULL) != TRUE) {
            if (gst_element_link_many(queue8_, tcp_nvvidconv_postosd_, tcp_caps_, tcp_encoder_, tcp_rtppay_, tcp_server_sink_, NULL) != TRUE)
            {
                g_printerr("Failed to link TCP Sink elements. 1111111111111111111111111111\n");
                return false;
            }
            if (!connect_src_sink_pads(sinktee_, queue8_))
            {
                g_printerr("Failed to connect TCP Sink elements. 2222222222222222222222222222\n");
                return false;
            }
            g_print("Pipeline::link_pipeline_elements(): TCP Sink enabled.\n");
        }
        if (configs_["rtsp-sink"]["udp-sink"]["enable"].as<int>())
        {
            // Connect sinktee to UDP Sink
            if (gst_element_link_many(queue7_, nvvidconv_postosd_, udp_caps_, encoder_, rtppay_, udp_sink_, NULL) != TRUE)
            {
                return false;
            }
            if (!connect_src_sink_pads(sinktee_, queue7_))
            {
                return false;
            }
            g_print("Pipeline::link_pipeline_elements(): UDP Sink enabled.\n");
        }
    }
    return true;
}

bool Pipeline::create_rtsp_server()
{
    // Create a server instance
    server_ = gst_rtsp_server_new();
    g_object_set(server_, "service", configs_["rtsp-sink"]["rtsp-server"]["rtsp-port"].as<std::string>().c_str(), NULL);

    // Get the mount points for this server, every server has a default object
    // that be used to map uri mount points to media factories
    mounts_ = gst_rtsp_server_get_mount_points(server_);

    // Make a media factory for a test stream. The default media factory can use
    // gst-launch syntax to create pipelines.
    // Any launch line works as long as it contains elements named pay%d. Each
    // element with pay%d names will be a stream.
    char rtsp_pipeline[1024] = {0};
    sprintf(rtsp_pipeline, configs_["rtsp-sink"]["rtsp-server"]["pipeline"].as<std::string>().c_str(),
            configs_["rtsp-sink"]["rtsp-server"]["udp-port"].as<unsigned short int>(),
            configs_["rtsp-sink"]["rtsp-server"]["encoding-name"].as<std::string>().c_str());
    // g_print("RTSP Pipeline: %s\r\n", rtsp_pipeline);

    factory_ = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory_, rtsp_pipeline);
    gst_rtsp_media_factory_set_shared(factory_, true);
    gst_rtsp_media_factory_set_enable_rtcp(factory_, configs_["rtsp-sink"]["rtsp-server"]["enable-rtcp"].as<int>());

    // Attach the factory to the mount point
    gst_rtsp_mount_points_add_factory(mounts_, configs_["rtsp-sink"]["rtsp-server"]["mountpoint"].as<std::string>().c_str(), factory_);

    // Don't need the ref to the mapper anymore
    g_object_unref(mounts_);

    // Attach the server to the default maincontext
    gst_rtsp_server_attach(server_, NULL);

    // Print information
    g_print("Output Stream will be ready at rtsp://127.0.0.1:%s%s\n", configs_["rtsp-sink"]["rtsp-server"]["rtsp-port"].as<std::string>().c_str(), configs_["rtsp-sink"]["rtsp-server"]["mountpoint"].as<std::string>().c_str());

    return true;
}

bool Pipeline::create_rtsp_server(const YAML::Node &configs)
{
    // Create a server instance
    server_ = gst_rtsp_server_new();
    // g_object_set(server_, "service", configs["rtsp-server"]["rtsp-port"].as<std::string>().c_str(), NULL);
    gst_rtsp_server_set_service(server_, configs["rtsp-server"]["rtsp-port"].as<std::string>().c_str());

    // Get the mount points for this server, every server has a default object
    // that be used to map uri mount points to media factories
    mounts_ = gst_rtsp_server_get_mount_points(server_);

    // Make a media factory for a test stream. The default media factory can use
    // gst-launch syntax to create pipelines.
    // Any launch line works as long as it contains elements named pay%d. Each
    // element with pay%d names will be a stream.
    char rtsp_pipeline[1024] = {0};
    sprintf(rtsp_pipeline, configs["rtsp-server"]["pipeline"].as<std::string>().c_str(),
            configs["rtsp-server"]["sink-port"].as<unsigned short int>(),
            configs["rtsp-server"]["encoding-name"].as<std::string>().c_str());
    // sprintf(rtsp_pipeline, configs["rtsp-server"]["pipeline"].as<std::string>().c_str(),
    //         configs["rtsp-server"]["sink-port"].as<unsigned short int>());
    g_print("RTSP Pipeline: %s\r\n", rtsp_pipeline);

    factory_ = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory_, rtsp_pipeline);
    gst_rtsp_media_factory_set_buffer_size(factory_, 1920 * 1080 * 20 * 4);
    gst_rtsp_media_factory_set_shared(factory_, false);
    gst_rtsp_media_factory_set_transport_mode(factory_, GST_RTSP_TRANSPORT_MODE_PLAY);
    gst_rtsp_media_factory_set_enable_rtcp(factory_, configs["rtsp-server"]["enable-rtcp"].as<int>());

    // Attach the factory to the mount point
    gst_rtsp_mount_points_add_factory(mounts_, configs["rtsp-server"]["mountpoint"].as<std::string>().c_str(), factory_);

    // Don't need the ref to the mapper anymore
    g_object_unref(mounts_);

    // Attach the server to the default maincontext
    gst_rtsp_server_attach(server_, NULL);

    // Print information
    g_print("Output Stream will be ready at rtsp://127.0.0.1:%s%s\n", configs["rtsp-server"]["rtsp-port"].as<std::string>().c_str(), configs["rtsp-server"]["mountpoint"].as<std::string>().c_str());

    return true;
}

void Pipeline::add_source(std::string uri, std::string camera_id, int pad_index, std::string description, bool use_nvurisrcbin, bool readding)
{
    GstElement *source_bin = NULL;
    // Create source bin
    if (readding) {
        source_bin = create_source_bin(pad_index, uri, use_nvurisrcbin, this->configs_);
    } else {
        source_bin = create_source_bin(num_sources_, uri, use_nvurisrcbin, this->configs_);
    }

    if (!source_bin)
    {
        g_printerr("Failed to create source bin. Exiting.\n");
        return;
    }
    // Save parameters for this stream
    streams_[pad_index].uri = uri;
    streams_[pad_index].camera_id = camera_id;
    streams_[pad_index].pad_index = pad_index;
    streams_[pad_index].description = description;
    streams_[pad_index].frameCount = 0;
    streams_[pad_index].noFrameCount = 0;
    streams_[pad_index].source_bin = source_bin;
    streams_[pad_index].reset = StreamResetStates::NONE;

    // Add new source to the pipeline
    gst_bin_add(GST_BIN(pipeline_), source_bin);
    // Connect muxer to source_bin src pad
    if (!connect_muxer_src_pads(streammux_, source_bin, pad_index))
    {
        g_printerr("Failed to connect muxer to source bin. Exiting.\n");
        return;
    }

    if (readding) {
        GstStateChangeReturn state_return; 
        state_return =
            gst_element_set_state (source_bin, GST_STATE_PLAYING);
        switch (state_return) {
            case GST_STATE_CHANGE_SUCCESS:
            g_print ("STATE CHANGE SUCCESS\n\n");
            break;
            case GST_STATE_CHANGE_FAILURE:
            g_print ("STATE CHANGE FAILURE\n\n");
            break;
            case GST_STATE_CHANGE_ASYNC:
            g_print ("STATE CHANGE ASYNC\n\n");
            state_return =
                gst_element_get_state (source_bin, NULL, NULL,
                GST_CLOCK_TIME_NONE);
            break;
            case GST_STATE_CHANGE_NO_PREROLL:
            g_print ("STATE CHANGE NO PREROLL\n\n");
            break;
            default:
            break;
        }
    } else {
        // Count source
        ++num_sources_;
    }
}

void Pipeline::delete_source(g2f::StreamSourceInfo stream)
{
    GstStateChangeReturn state_return;
    gchar pad_name[16];
    GstPad *sinkpad = NULL;

    state_return = gst_element_set_state (stream.source_bin, GST_STATE_NULL);
    switch (state_return) {
    case GST_STATE_CHANGE_SUCCESS:
        g_print ("STATE CHANGE SUCCESS\n\n");
        g_snprintf (pad_name, 15, "sink_%u", stream.pad_index);
        sinkpad = gst_element_get_static_pad (streammux_, pad_name);
        gst_pad_send_event (sinkpad, gst_event_new_eos ());
        gst_pad_send_event (sinkpad, gst_event_new_flush_stop (FALSE));
        gst_element_release_request_pad (streammux_, sinkpad);
        g_print ("STATE CHANGE SUCCESS %p\n\n", sinkpad);
        gst_object_unref (sinkpad);
        gst_bin_remove (GST_BIN (pipeline_), stream.source_bin);
        // source_id--;
        // g_num_sources--;
        break;
    case GST_STATE_CHANGE_FAILURE:
        g_print ("STATE CHANGE FAILURE\n\n");
        break;
    case GST_STATE_CHANGE_ASYNC:
        g_print ("STATE CHANGE ASYNC\n\n");
        g_snprintf (pad_name, 15, "sink_%u", stream.pad_index);
        sinkpad = gst_element_get_static_pad (streammux_, pad_name);
        gst_pad_send_event (sinkpad, gst_event_new_eos ());
        gst_pad_send_event (sinkpad, gst_event_new_flush_stop (FALSE));
        gst_element_release_request_pad (streammux_, sinkpad);
        g_print ("STATE CHANGE ASYNC %p\n\n", sinkpad);
        gst_object_unref (sinkpad);
        gst_bin_remove (GST_BIN (pipeline_), stream.source_bin);
        // source_id--;
        // g_num_sources--;
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        g_print ("STATE CHANGE NO PREROLL\n\n");
        break;
    default:
        break;
    }
}
