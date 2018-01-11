#include <assert.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xpath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libnotify/notify.h>

#define UPD_INTERVAL 20

typedef struct {
	xmlChar *msg;
	xmlChar *datetime;
	xmlChar *name;
	xmlChar *num;      
} Post;

typedef struct {
	xmlChar *bp;
	size_t size;
} Buffer;

size_t write_data (char *contents, size_t size, size_t nmemb, void *userdata);
Post *get_posts(xmlDocPtr doc);
Post *get_post(xmlDocPtr doc, xmlNodePtr node);
xmlNodePtr get_node_by_attr(xmlNodePtr node, xmlChar* attr, xmlChar* attr_val);
void print_post(Post *post);
void print_thread(xmlDocPtr doc);
void print_new_posts(xmlDocPtr doc);
void free_post(Post *post);
void parse_message(char *msg, xmlNodePtr node);

#define get_node_value(node)\
	xmlNodeListGetString(doc, node->nodesetval->nodeTab[0]->xmlChildrenNode, 1)

int main (int argc, xmlChar *argv[]) {
	CURL *handle;
	CURLcode res;
	Buffer buf;
	htmlDocPtr doc;
	int rc;

	if (argc != 2) {
		fprintf (stderr, "Usage: 4notify URL\n");
		return 1;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, argv[1]);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	buf.bp = NULL;
	buf.size = 0;
	notify_init("4notify");
	while (1) {
		if ((res = curl_easy_perform(handle)) != CURLE_OK) {
			fprintf(stderr, "Error: %s\n", curl_easy_strerror(res));
			return 1;
		}

		doc = htmlReadDoc(buf.bp, NULL, "utf-8", XML_PARSE_NOERROR);
		if (!doc) {
			fprintf(stderr, "Could not parse the document.\n");
			return 1;
		}

		print_new_posts(doc);
		xmlFreeDoc(doc);
		buf.size = 0;
		sleep(UPD_INTERVAL);
	}

	
	return 0;
}

void callback_test(NotifyNotification *n, char *action, gpointer user_data)
{
	printf("%s\n", action);
}

void
print_new_posts (xmlDocPtr doc)
{
	char path[100];
	static char *last_num = NULL;
	//Stores the time of the last post.
	int node_num;
	int notif_count;
	char summary[100];
	xmlNodePtr *node_tab;
	Post *post;
	NotifyNotification *notif;

	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	context = xmlXPathNewContext(doc);
	//If it's checking the thread for the first time, store the time
	//of the OP post.
	if (!last_num) {
		sprintf(path, "(//*[@class=\"postContainer replyContainer\"])\
				[last()]\
				/descendant::*[@class=\"dateTime\"]\
				/attribute::data-utc");
		result = xmlXPathEvalExpression(path, context);
		last_num = get_node_value(result);
		xmlXPathFreeObject(result);
		return;
	}

	//Get all the new posts.
	sprintf(path, "//*[@class=\"dateTime\" and @data-utc>%s]\
			/ancestor::*[@class=\"postContainer replyContainer\"]", 
		last_num);
	result = xmlXPathEvalExpression(path, context);

	//Print the posts.
	assert(result); 
	if (result->nodesetval) {
		node_num = result->nodesetval->nodeNr;
		node_tab = result->nodesetval->nodeTab;
		while(node_num--) {
			post = get_post(doc, *node_tab);
			sprintf(summary, "Name:%s %s #%s", 
				post->name, post->datetime, post->num);	

			notif = notify_notification_new(summary, 
							post->msg, 
							NULL);
			notify_notification_set_timeout(notif, 10000);
			notify_notification_add_action(	notif,
							"test",
							"Test",
							callback_test,
							NULL,
							NULL);
			notify_notification_show(notif, 0);

			free_post(post);
			node_tab++;	
		}
	}
	xmlXPathFreeObject(result);

	//Get the time of the newest post.
	sprintf(path, "(//*[@class=\"postContainer replyContainer\"])[last()]\
			/descendant::*[@class=\"dateTime\"]\
			/attribute::data-utc");
	result = xmlXPathEvalExpression(path, context);
	last_num = get_node_value(result);

	xmlXPathFreeObject(result);
	xmlXPathFreeContext(context);
}

size_t 
write_data (char *contents, size_t size, size_t nmemb, void *userdata) 
{
	Buffer *buf = (Buffer *) userdata;
	size_t rsize = size*nmemb; 
	static int count = 0;

	assert(rsize != 0);
	buf->bp = realloc(buf->bp, buf->size+rsize);
	strncpy(buf->bp+buf->size, contents, rsize);
	buf->size += rsize;

	return rsize;
}

void 
print_thread(xmlDocPtr doc)
{

	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodes;
	Post *post;

	context = xmlXPathNewContext(doc);
	result = xmlXPathEvalExpression("//*[@class=\"postContainer\
					replyContainer\"]", 
					context);
	nodes = result->nodesetval;	

	while(nodes->nodeNr--) {
		post = get_post(doc, *nodes->nodeTab);
		printf("Name:%s %s #%s\n\t%s\n\n", post->name, post->datetime,
				  		   post->num,  post->msg);	
		free_post(post);
		nodes->nodeTab++;	
	}
}

Post*
get_post(xmlDocPtr doc, xmlNodePtr node)
{
	Post *post;
	xmlNodePtr field;
	xmlNodeSetPtr node_list;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	int node_num;
	int post_num;
	xmlNodePtr *node_tab;
	int len;

	post = calloc(1, sizeof(Post));
	post->msg = malloc(5000*sizeof(char));
	post->msg[0] = '\0';
	context = xmlXPathNewContext(doc);
	xmlXPathSetContextNode(node, context);

	result = xmlXPathEvalExpression(".//*[@class=\"name\"]", context);
	post->name = get_node_value(result);
	xmlXPathFreeObject(result);
	
	result = xmlXPathEvalExpression(".//*[@class=\"dateTime\"]", context);
	post->datetime = get_node_value(result);
	xmlXPathFreeObject(result);

	result = xmlXPathEvalExpression(".//*[@title=\"Reply to this post\"]", 
					context);
	post->num = get_node_value(result);
	post_num = atoi(post->num);
	xmlXPathFreeObject(result);

	result = xmlXPathEvalExpression(".//*[@class=\"postMessage\"]", 
					context);
	len = -1;
	assert(result);
	if (result->nodesetval) {
		node_num = result->nodesetval->nodeNr;
		node_tab = result->nodesetval->nodeTab;
		while (node_num--) {
			parse_message(post->msg, *node_tab);
		}
	}
	xmlXPathFreeObject(result);

	return post;
}

xmlNodePtr 
get_node_by_attr(xmlNodePtr node, xmlChar* attr_name, xmlChar* attr_val)
{
	xmlChar *value;
	assert(node);
	for (node = node->children; node; node = node->next) {
		value = xmlGetProp(node, attr_name);
		if (value && !xmlStrcmp(value, attr_val))
			return node;
	}
	return NULL;
}

void free_post(Post *post)
{
	free(post->msg);
	free(post->datetime);
	free(post->name);
	free(post->num);
	free(post);
}

void parse_message(char *msg, xmlNodePtr node)
{
	int size;

	assert(node);

	for (node = node->children; node; node = node->next) { 
		if (node->type == XML_ELEMENT_NODE
		&& strcmp(node->name, "br")) 
			parse_message(msg, node);
		else if (node->type == XML_TEXT_NODE)
			strcat(msg, node->content);
		else
			strcat(msg, "\n");
	}
}
