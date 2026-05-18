#pragma once
#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "mongoose.h"

// 请求网关数据
void getGatewayTags(const char* gateway_ip);

#endif