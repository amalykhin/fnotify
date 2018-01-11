#include <assert.h>
#include <curl/curl.h>
#include <gumbo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_LINE_LEN 80

//#define DEBUG
#ifdef DEBUG
#define dump_node(node)\
		printf("%s %s=%s\n", TidyNodeGetName(node),\
		tidyAttrName(tidyAttrFirst(node)),\
		tidyAttrValue(tidyAttrFirst(node)));
#else 
#define dump_node(node) ;
#endif

typedef GumboNode* Node;
 //why can't just use node+1?
#define get_next_node(node)\
		(node->index_within_parent+1\
		< node->parent->v.element.children.length\
			? node->parent->v.element.children.data[node->index_within_parent+1]: NULL)
#define get_child_node(node)\
	       node->v.element.children.length > 0\
			?((Node)node->v.element.children.data[0]): NULL
#define get_parent_node(node)\
		(node->parent)

typedef struct {
	char *datetime;
	char *msg;
	char *name;
	char *num;
	char *quote;
} Post;

typedef struct {
	char *bp;
	size_t size;
} Buffer;

size_t write_data (char *contents, size_t size, size_t nmemb, void *userdata);
Post *get_post(Node node);
Node get_node_by_attr(Node node, char* attr, char* attr_val);
void print_post(Post *post);
void print_thread(Node node);


int main (int argc, char *argv[]) {
	CURL *handle;
	CURLcode res;
	Buffer buf;
	GumboOutput* doc;
	
	int rc;

	//buf.content = NULL;
	//buf.size = 0;

	if (argc != 2) {
		fprintf (stderr, "Usage: 4notify URL\n");
		return 1;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL,           argv[1]);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA,     &buf);
	curl_easy_setopt(handle, CURLOPT_USERAGENT,     "libcurl-agent/1.0");

	buf.bp = NULL;
	buf.size = 0;
	printf("Getting HTML...\n");
	if ((res = curl_easy_perform(handle)) != CURLE_OK) {
		fprintf(stderr, "Error: %s\n", curl_easy_strerror(res));
		return 1;
	}

	buf.bp[buf.size] = '\0';
	//puts(buf.bp);

	curl_easy_cleanup(handle);
	curl_global_cleanup();

	doc = gumbo_parse(buf.bp);
	if (!doc) {
		fprintf(stderr, "Could not parse the document.\n");
		return 1;
	}

//	doc = tidyCreate();
//	printf("Parsing...\n");
//	rc = tidyParseBuffer(doc, &buf);
//	if (rc >= 0) {
//		printf("Tidying the markup...\n");
//		rc = tidyCleanAndRepair(doc);
//	}
//	if (rc >= 0) {
		printf("Dumping the posts...\n");
		print_thread(doc->root);
//	}

	gumbo_destroy_output(&kGumboDefaultOptions, doc);
	
	return 0;
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

void print_thread(Node node)
{
	Post *post;
	int i, j;

	//Get the <body> element
	node = (Node)node->v.element.children.data[1];
	//Get the post container
	node = get_node_by_attr(node, "name",  "delform");
	node = get_node_by_attr(node, "class", "board");
	node = get_node_by_attr(node, "class", "thread");
	node = get_node_by_attr(node, "class", "postContainer opContainer");

	while (node) {
		post = get_post(node);
		printf("-------------[ Name: %10s | Date&Time: %s | #%s ]-------------\n", 
				post->name, post->datetime, post->num);
		if (post->quote[0])
			printf("\t%s\n", post->quote);
		if (post->msg[0]) {
			i = strlen(post->msg) > MSG_LINE_LEN ? MSG_LINE_LEN : 1; 
			for (j = 0; post->msg[i]; i++) {
				if (post->msg[i] == ' ' && i-j >= MSG_LINE_LEN) {
					post->msg[i] = '\n';
				}
				if (post->msg[i] == '\n')
					j = i;
			}
			printf("\t%s\n", post->msg);
		} 
		putchar('\n');
		free (post);
		node = get_next_node(node);
	}
	
}

//Govno
/*
char*
get_node_message(Node node)
{
	int size, len, rem;
	char *str;

	size = 100;
	rem = size;
	str = calloc(rem, sizeof(char));
	node = node ? get_child_node(node) : NULL;
	while (node) {
		if (node->type == GUMBO_NODE_TEXT) {
			len = strlen(node->v.text.text) + 1;
			if (rem < len) {
				str = realloc(str, size*=2);
				rem += size;
			}
			strcat(strcat(str, node->v.text.text)," " );
			rem -= len;
		} else if (node->type == GUMBO_NODE_ELEMENT 
				&& node->v.element.tag == GUMBO_TAG_BR) {
			if (rem < 1) {
				str = realloc(str, size*=2);
				rem += size;
			}
			strcat(str, "\n\t");
		} else if (node->type == GUMBO_NODE_ELEMENT) {
			node = get_child_node(node);
			//It would be great to free that memory
			strcat(strcat(str, get_node_message(node)));
			node = get_parent_node(node);
		}		
		node = get_next_node(node);
	}

	return str;
}
*/

Post*
get_post(Node node)
{
#define get_node_value(node, var)\
		node = node ? get_child_node(node) : NULL;\
		while (node) {\
			if (node->type == GUMBO_NODE_TEXT)\
				strcat(strcat(var, node->v.text.text)," " );\
			node = get_next_node(node);\
		}
	Post *post;
	Node field;

	post = calloc(1, sizeof(Post));
	post->datetime = calloc(15, sizeof(char));
	post->name     = calloc(20, sizeof(char));
	post->num      = calloc(10, sizeof(char));
	post->msg      = calloc(1000, sizeof(char));
	post->quote    = calloc(500, sizeof(char));
	field = node;

	if (!(node = get_node_by_attr(node, "class", "post reply")))
		node = get_node_by_attr(field, "class", "post op");

	//Get post info node
	node = get_node_by_attr(node, "class", "postInfo desktop");

	//Get post name
	node = get_node_by_attr(node, "class", "nameBlock");
	field = get_node_by_attr(node, "class", "name");
	get_node_value(field, post->name);
	node = get_parent_node(node);
	
	//Get post date and time
	field = get_node_by_attr(node, "class", "dateTime");
	get_node_value(field, post->datetime);

	//Get post number
	node = get_node_by_attr(node, "class", "postNum desktop");
	field = get_node_by_attr(node, "title", "Reply to this post");
	get_node_value(field, post->num);
	node = get_parent_node(node);
	node = get_parent_node(node);

	//Get post message
	node = get_node_by_attr(node, "class", "postMessage");
	field = node;
	get_node_value(field, post->msg);

	//Get quotes
	field = get_node_by_attr(node, "class", "quote");
	get_node_value(field, post->quote);

	return post;
#undef get_node_value
}

Node 
get_node_by_attr(Node node, char* attr_name, char* attr_val)
{
	char *value;
	GumboAttribute *attr;
	for (node = get_child_node(node); node; node = get_next_node(node)) {
		dump_node(node);
		if (node->type != GUMBO_NODE_ELEMENT) 
			continue;
		attr = gumbo_get_attribute(&node->v.element.attributes, attr_name);
		if (attr && !strcmp(attr->value, attr_val))
			return node;
	}
	return NULL;
}
