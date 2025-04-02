#!/bin/bash

listen_port=$1
destination_ip=$2
destination_port=$3

socat TCP-LISTEN:"${listen_port}",fork TCP:"${destination_ip}":"${destination_port}"
