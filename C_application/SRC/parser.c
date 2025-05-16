#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "parser.h"

// Convert MAC string to byte array
static int string_to_mac(const char* mac_str, uint8_t* mac_bytes) {
    return sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac_bytes[0], &mac_bytes[1], &mac_bytes[2],
                  &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]) == 6 ? 0 : -1;
}

int parse_xml(const char *filename, Instance **instances, int *instance_count, int *sampling_time) {
    xmlDoc *doc = xmlReadFile(filename, NULL, 0);
    if (!doc) {
        fprintf(stderr, "Failed to parse %s\n", filename);
        return -1;
    }

    xmlNode *root = xmlDocGetRootElement(doc);
    if (!root || xmlStrcmp(root->name, (const xmlChar *)"instances")) {
        fprintf(stderr, "Invalid root element\n");
        xmlFreeDoc(doc);
        return -1;
    }

    // Count instances
    int count = 0;
    for (xmlNode *node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && !xmlStrcmp(node->name, (const xmlChar *)"instance"))
            count++;
    }
    *instance_count = count;
    *instances = malloc(count * sizeof(Instance));
    if (!*instances) {
        xmlFreeDoc(doc);
        return -1;
    }

    // Parse each instance
    int i = 0;
    for (xmlNode *node = root->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, (const xmlChar *)"instance")) continue;

        Instance *inst = &(*instances)[i++];
        memset(inst, 0, sizeof(Instance));

        for (xmlNode *child = node->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;

            if (!xmlStrcmp(child->name, (const xmlChar *)"SV_publisher")) {
                for (xmlNode *sv = child->children; sv; sv = sv->next) {
                    if (sv->type != XML_ELEMENT_NODE) continue;
                    xmlChar *content = xmlNodeGetContent(sv);
                    if (!xmlStrcmp(sv->name, (const xmlChar *)"appId")) {
                        inst->appId = (uint16_t)strtol((char *)content, NULL, 0); // Hex or decimal
                    } else if (!xmlStrcmp(sv->name, (const xmlChar *)"svInterface")) {
                        inst->svInterface = strdup((char *)content);
                    } else if (!xmlStrcmp(sv->name, (const xmlChar *)"svIDs")) {
                        int svid_count = 0;
                        for (xmlNode *svid = sv->children; svid; svid = svid->next) {
                            if (svid->type == XML_ELEMENT_NODE && !xmlStrcmp(svid->name, (const xmlChar *)"svID"))
                                svid_count++;
                        }
                        inst->svIDs = malloc(svid_count * sizeof(char *));
                        inst->svIDCount = svid_count;
                        int j = 0;
                        for (xmlNode *svid = sv->children; svid; svid = svid->next) {
                            if (svid->type == XML_ELEMENT_NODE && !xmlStrcmp(svid->name, (const xmlChar *)"svID")) {
                                xmlChar *svid_content = xmlNodeGetContent(svid);
                                inst->svIDs[j++] = strdup((char *)svid_content);
                                xmlFree(svid_content);
                            }
                        }
                    }
                    xmlFree(content);
                }
            } else if (!xmlStrcmp(child->name, (const xmlChar *)"GOOSE_subscriber")) {
                for (xmlNode *goose = child->children; goose; goose = goose->next) {
                    if (goose->type != XML_ELEMENT_NODE) continue;
                    xmlChar *content = xmlNodeGetContent(goose);
                    if (!xmlStrcmp(goose->name, (const xmlChar *)"GOOSEInterface")) {
                        inst->gooseInterface = strdup((char *)content);
                    } else if (!xmlStrcmp(goose->name, (const xmlChar *)"GOOSEappId")) {
                        inst->gooseAppId = (uint16_t)strtol((char *)content, NULL, 16); // Hex
                    } else if (!xmlStrcmp(goose->name, (const xmlChar *)"dstMac")) {
                        string_to_mac((char *)content, inst->dstMac);
                    } else if (!xmlStrcmp(goose->name, (const xmlChar *)"goCbRef")) {
                        inst->goCbRef = strdup((char *)content);
                    }
                    xmlFree(content);
                }
            } else if (!xmlStrcmp(child->name, (const xmlChar *)"scenarioConfigFile")) {
                xmlChar *content = xmlNodeGetContent(child);
                inst->scenarioConfigFile = strdup((char *)content);
                xmlFree(content);
            }
        }
    }

    // Parse Sampling_time
    for (xmlNode *node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && !xmlStrcmp(node->name, (const xmlChar *)"Sampling_time")) {
            xmlChar *content = xmlNodeGetContent(node);
            *sampling_time = atoi((char *)content);
            xmlFree(content);
            break;
        }
    }

    xmlFreeDoc(doc);
    return 0;
}

void free_instances(Instance *instances, int instance_count) {
    for (int i = 0; i < instance_count; i++) {
        free(instances[i].svInterface);
        for (int j = 0; j < instances[i].svIDCount; j++) free(instances[i].svIDs[j]);
        free(instances[i].svIDs);
        free(instances[i].gooseInterface);
        free(instances[i].goCbRef);
        free(instances[i].scenarioConfigFile);
    }
    free(instances);
}