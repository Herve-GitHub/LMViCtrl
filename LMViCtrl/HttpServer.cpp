#include "HttpServer.h"
#include "mongoose.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

// 引入 cJSON 解析库（你项目里应该已有，没有我再给你）
#include "cJSON.h"

static char g_json_buf[200000] = { 0 };
static int g_json_len = 0;
static bool g_http_done = false;

static void http_handler(struct mg_connection* c, int ev, void* ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;

        g_json_len = hm->body.len;
        if (g_json_len >= (int)sizeof(g_json_buf))
            g_json_len = sizeof(g_json_buf) - 1;

        memcpy(g_json_buf, hm->body.buf, g_json_len);
        g_json_buf[g_json_len] = 0;

        g_http_done = true;
    }
}

// 递归解析 JSON，提取所有 id 字段（如 Device2.tag0001）
void parse_children(cJSON* node, FILE* file)
{
    cJSON* child;
    cJSON_ArrayForEach(child, node)
    {
        cJSON* id = cJSON_GetObjectItem(child, "id");
        if (id && cJSON_IsString(id) && id->valuestring != NULL)
        {
            fprintf(file, "%s\n", id->valuestring);
        }

        cJSON* children = cJSON_GetObjectItem(child, "children");
        if (children && cJSON_IsArray(children))
        {
            parse_children(children, file);
        }
    }
}

void getGatewayTags()
{
    g_json_len = 0;
    g_http_done = false;
    memset(g_json_buf, 0, sizeof(g_json_buf));

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    mg_connection* conn = mg_http_connect(&mgr, "http://192.168.0.85", http_handler, NULL);
    if (!conn) {
        mg_mgr_free(&mgr);
        return;
    }

    mg_printf(conn,
        "POST /scada/getTagsTree HTTP/1.1\r\n"
        "Host: 192.168.0.85\r\n"
        "Content-Type: application/json\r\n"
        "token: scadaToken\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n");

    // 等待数据接收完成
    int timeout = 5000;
    int elapsed = 0;
    while (!g_http_done && elapsed < timeout) {
        mg_mgr_poll(&mgr, 1);
        Sleep(1);
        elapsed++;
    }

    mg_mgr_free(&mgr);

    // ==============================
    // 解析 JSON，提取所有点位 ID
    // ==============================
    cJSON* root = cJSON_Parse(g_json_buf);
    if (root == NULL)
    {
        printf("JSON 解析失败\n");
        return;
    }

    // 保存到【当前程序目录】的 tag_list.txt
    FILE* f = fopen("./tag_list.txt", "w");
    if (!f) return;

    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (data && cJSON_IsArray(data))
    {
        cJSON* item;
        cJSON_ArrayForEach(item, data)
        {
            cJSON* children = cJSON_GetObjectItem(item, "children");
            if (children && cJSON_IsArray(children))
            {
                parse_children(children, f);
            }
        }
    }

    fclose(f);
    cJSON_Delete(root);
}