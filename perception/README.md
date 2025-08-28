# ds-multicamera-tracking
Multicamera Tracking using NVIDIA Deepstream

Testing

## Unix Timestamp Converter
https://www.unixtimestamp.com/

# A little about Metropolis v2.1

The inspiration of our code was based in metropolis from NVIDIA. Below are some links to view:

[Introduction](https://docs.nvidia.com/mms/text/MDX_Introduction.html)

[Download and Install](https://docs.nvidia.com/mms/text/MDX_Download_install.html)

[Multi Camera Tracking App](https://docs.nvidia.com/mms/text/MDX_Multi_Camera_Tracking_App.html#overview)


## Getting Metropolis

The next is using *NCG CLI*:

a. For the Standalone Deployment package:
```bash
ngc registry resource download-version "nfgnkvuikvjm/mdx-v2-0/metropolis-apps-standalone-deployment:v2.1-05082024"
```
b. For the Sample Input Data:
```bash
ngc registry resource download-version "nfgnkvuikvjm/mdx-v2-0/metropolis-apps-sample-input-data:v2.1-04252024"
```

## Unix Timestamp Converter
https://www.unixtimestamp.com/

## Kafka Topic Subscription

1. Connect to the Kafka container with:
```bash
docker exec -u root -it mdx-kafka /bin/bash
```

2. Run the command below:
```bash
kafka-console-consumer --bootstrap-server localhost:9092 --topic mdx-raw --from-beginning
```

## Apache Zookeeper

ZooKeeper is a centralized service for maintaining configuration information, naming, providing distributed synchronization, and providing group services. All of these kinds of services are used in some form or another by distributed applications. Each time they are implemented there is a lot of work that goes into fixing the bugs and race conditions that are inevitable. Because of the difficulty of implementing these kinds of services, applications initially usually skimp on them, which make them brittle in the presence of change and difficult to manage. Even when done correctly, different implementations of these services lead to management complexity when the applications are deployed.

[Link to ZooKeeper Webpage](https://zookeeper.apache.org/)


# Base64 for C++

https://github.com/tobiaslocker/base64/tree/master