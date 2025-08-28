#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <cstring>
#include <vector>
#include <opencv2/opencv.hpp>


namespace g2f
{
    /**
     * @brief Interface for base information
     */
    class BaseInfo
    {
    public:
        /**
         * @brief Get the message associated with the information
         * @return The message
         */
        virtual std::string getMessage() const = 0;

        /**
         * @brief Get the topic associated with the information
         * @return The topic
         */
        virtual std::string getTopic() const = 0;

    protected:
        std::string topic_;
        std::string message_;
    };

    /**
     * @brief Class to store the information of the object
     */
    class ObjInfo : public std::enable_shared_from_this<ObjInfo>
    {
    public:
        ObjInfo(std::string camera_id="", std::string description="", int muxer_pad_index=-1, int frame_num=-1, int class_id=-1, unsigned int object_id=-1, 
            double confidence=0.0, int bbox_top=0, int bbox_left=0, int bbox_width=0, int bbox_height=0, 
            unsigned int embeddings_size=256, float *embeddings = nullptr, std::vector<std::string> roi = {}, unsigned long long int ts_nanoseconds = 0) 
        {
            ts_nanoseconds_ = ts_nanoseconds;
            camera_id_ = camera_id;
            description_ = description;
            muxer_pad_index_ = muxer_pad_index;
            frame_num_ = frame_num;
            class_id_ = class_id;
            object_id_ = object_id;
            confidence_ = confidence;
            bbox_top_ = bbox_top; 
            bbox_left_ = bbox_left;
            bbox_height_ = bbox_height;
            bbox_width_ = bbox_width;
            embeddings_size_ = embeddings_size;
            embeddings_ = embeddings;
            if (embeddings_ != nullptr) {
                embeddings_ = new float[embeddings_size_];
                memcpy(embeddings_, embeddings, sizeof(float) * embeddings_size_);
                // // cudaMemcpy(embeddings_, embeddings, sizeof(float) * embedding_size_);
                // for (int i = 0; i < embedding_size_; i++)
                // {
                //     embeddings_[i] = embeddings[i];
                // }
            }
            roi_ = roi;
        };
        //
        // Copy Constructor
        ObjInfo(const ObjInfo &obj)
        {
            ts_nanoseconds_ = obj.ts_nanoseconds_;
            camera_id_ = obj.camera_id_;
            description_ = obj.description_;
            muxer_pad_index_ = obj.muxer_pad_index_;
            frame_num_ = obj.frame_num_;
            class_id_ = obj.class_id_;
            object_id_ = obj.object_id_;
            confidence_ = obj.confidence_;
            bbox_top_ = obj.bbox_top_;
            bbox_left_ = obj.bbox_left_;
            bbox_height_ = obj.bbox_height_;
            bbox_width_ = obj.bbox_width_;
            embeddings_size_ = obj.embeddings_size_;
            embeddings_ = obj.embeddings_;
            if (embeddings_ != nullptr)
            {
                embeddings_ = new float[embeddings_size_];
                memcpy(embeddings_, obj.embeddings_, sizeof(float) * embeddings_size_);
            }
            roi_ = obj.roi_;
        };
        //
        // Move Constructor
        ObjInfo(ObjInfo &&obj)
        {
            ts_nanoseconds_ = obj.ts_nanoseconds_;
            camera_id_ = obj.camera_id_;
            description_ = obj.description_;
            muxer_pad_index_ = obj.muxer_pad_index_;
            frame_num_ = obj.frame_num_;
            class_id_ = obj.class_id_;
            object_id_ = obj.object_id_;
            confidence_ = obj.confidence_;
            bbox_top_ = obj.bbox_top_;
            bbox_left_ = obj.bbox_left_;
            bbox_height_ = obj.bbox_height_;
            bbox_width_ = obj.bbox_width_;
            embeddings_size_ = obj.embeddings_size_;
            embeddings_ = obj.embeddings_;
            roi_ = obj.roi_;
            obj.embeddings_ = nullptr;
        };
        //
        // Destructor
        ~ObjInfo()
        {
            if (embeddings_ != nullptr) delete[] embeddings_;
        };

    public:
        // Camera ID
        std::string camera_id_;
        // Description
        std::string description_;
        // Multiplexer source pad index
        int muxer_pad_index_;
        // Frame number where the object was detected
        int frame_num_;
        // Class ID
        int class_id_;
        // Object ID given by the tracker
        unsigned int object_id_;
        // Confidence
        double confidence_;
        // Bounding Box data
        int bbox_top_;
        int bbox_left_;
        int bbox_width_;
        int bbox_height_;
        // Embedding size
        unsigned int embeddings_size_;
        // Embedding values
        float *embeddings_ = nullptr;
        // Roi list
        std::vector<std::string> roi_;
        // Unix Timestamp in nanoseconds
        unsigned long long int ts_nanoseconds_;
    };

    /**
     * @brief Class to store the information of the frame and send it through mqtt
     */
    class FrameInfo : public std::enable_shared_from_this<FrameInfo>
    {
    public:
        FrameInfo(unsigned long long int ts_nanoseconds = 0, std::string camera_id="", int frame_num=-1, cv::Mat frame = cv::Mat()) 
        {
            ts_nanoseconds_ = ts_nanoseconds;
            camera_id_ = camera_id;
            frame_num_ = frame_num;
            frame_ = frame;
        };
        //
        // Copy Constructor
        FrameInfo(const FrameInfo &obj)
        {
            ts_nanoseconds_ = obj.ts_nanoseconds_;
            camera_id_ = obj.camera_id_;
            frame_num_ = obj.frame_num_;
            frame_ = obj.frame_;
        };
        //
        // Move Constructor
        FrameInfo(FrameInfo &&obj)
        {
            ts_nanoseconds_ = obj.ts_nanoseconds_;
            camera_id_ = obj.camera_id_;
            frame_num_ = obj.frame_num_;
            frame_ = obj.frame_;
        };

        void setFrame(const cv::Mat& frame)
        {
            frame_ = frame;
        }

    public:
        // Unix Timestamp in nanoseconds
        unsigned long long int ts_nanoseconds_;
        // Camera ID
        std::string camera_id_;
        // Frame number where the object was detected
        int frame_num_;
        // Opencv Frame
        cv::Mat frame_;
    };

} // namespace g2f
