cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aws_aq)

list(APPEND EXTRA_COMPONENT_DIRS aws-iot)