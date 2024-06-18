/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Konrad Witaszczyk
 *
 * This software was developed by SRI International, the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology), and Capabilities Limited under Defense Advanced Research
 * Projects Agency (DARPA) Contract No. HR001123C0031 ("MTSS").
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

#include <map>
#include <string>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

typedef enum {
	NODE_TYPE_INVALID,
	NODE_TYPE_FUNCTION,
	NODE_TYPE_COMPARTMENT
} node_type_t;

typedef enum {
	FORMAT_TYPE_INVALID,
	FORMAT_TYPE_D3_ARC
} format_type_t;

void
usage(void)
{

	fprintf(stderr, "usage: cpg-reducer -n function|compartment -f d3-arc input-dot-file\n");
	exit(1);
}

static node_type_t
node_type_from_str(const char *str)
{

	if (strcmp(str, "function") == 0)
		return (NODE_TYPE_FUNCTION);
	else if (strcmp(str, "compartment") == 0)
		return (NODE_TYPE_COMPARTMENT);
	else
		return (NODE_TYPE_INVALID);
}

static format_type_t
format_type_from_str(const char *str)
{

	if (strcmp(str, "d3-arc") == 0)
		return (FORMAT_TYPE_D3_ARC);
	else
		return (FORMAT_TYPE_INVALID);
}

void
graph_remove_intra_edges(Agraph_t *g)
{
	Agnode_t *n, *nxtnode, *m;
	Agedge_t *e, *nxtout;
	const char *file_n, *file_m;
	bool isreduced;

	for (n = agfstnode(g); n != NULL; n = nxtnode) {
		nxtnode = agnxtnode(g, n);
		isreduced = false;

		file_n = agget(n, "file");
		assert(file_n != NULL);

		for (e = agfstout(g, n); e != NULL; e = nxtout) {
			nxtout = agnxtout(g, e);
			m = aghead(e);

			file_m = agget(m, "file");
			assert(file_m != NULL);

			if (strlen(file_n) == 0 || strlen(file_m) == 0 ||
			    strcmp(file_n, file_m) != 0) {
				/*
				 * Leave the edge with at least one node not
				 * associated with any file or nodes associated
				 * with different files.
				 */
				continue;
			}

			/*
			 * Remove the edge if it's within the same file.
			 */
			agdelete(g, e);
			isreduced = true;

			if (agdegree(g, m, true, true) > 0) {
				/*
				 * Leave the adjacent node if it still has some
				 * edges.
				 */
				continue;
			}

			/*
			 * Remove the adjacent node if it doesn't have any more
			 * edges.
			 */
			if (nxtnode == m)
				nxtnode = agnxtnode(g, nxtnode);
			agdelnode(g, m);
		}

		if (isreduced && agdegree(g, n, true, true) == 0) {
			/*
			 * The node was reduced and doesn't have any inter-file
			 * edges.
			 *
			 * Nodes without edges that haven't been reduced are
			 * left in CPG to indicate potential issues with an
			 * input CPG.
			 */
			agdelnode(g, n);
		}
	}
}

Agraph_t *
graph_merge_intra_nodes(Agraph_t *g)
{
	std::map<std::string, Agnode_t *> compartments;
	Agraph_t *h;
	Agnode_t *compartment, *n, *nxtnode;
	Agedge_t *e, *nxtout;
	char *file_n;

	h = agopen("kernel", Agstrictdirected, NULL);
	assert(h != NULL);

	for (n = agfstnode(g); n != NULL; n = nxtnode) {
		nxtnode = agnxtnode(g, n);

		file_n = agget(n, "file");
		assert(file_n != NULL);
if (strlen(file_n) == 0)
	continue;
		auto it = compartments.find(file_n);
		if (it == compartments.end()) {
			compartment = agnode(h, agstrdup(h, file_n), true);
			agset(compartment, "label", agstrdup(h, file_n));
			printf("label %s file %s\n", file_n, file_n);
			agset(compartment, "file", agstrdup(h, file_n));
			compartments.insert({file_n, compartment});
		} else {
			compartment = it->second;
		}

		for (e = agfstout(g, n); e != NULL; e = nxtout) {
			nxtout = agnxtout(g, e);
		}
	}

	return (h);
}

void
graph_print_d3_arc(Agraph_t *g)
{
	Agnode_t *n, *nxtnode;
	Agedge_t *e, *nxtout;
	const char *file, *labeln, *labelm, *value;
	int filelen, labelnlen, labelmlen;

	printf("{\n");

	printf("  \"nodes\": [\n");
	for (n = agfstnode(g); n != NULL; n = nxtnode) {
		nxtnode = agnxtnode(g, n);

		labeln = agget(n, "label");
		labelnlen = strlen(labeln);
		file = agget(n, "file");
		filelen = strlen(file);

		printf("    {\"id\": \"%.*s\", \"group\": \"%.*s%s\"}",
		    labelnlen > 2 ? labelnlen - 2 : 0,
		    labeln + 1,
		    filelen > 4 ? filelen - 4 : 0,
		    file + 1,
		    filelen == 0 ? "NONE" : "");
		if (nxtnode != NULL)
			printf(",");
		printf("\n");
	}
	printf("  ],\n");

	printf("  \"links\": [\n");
	for (n = agfstnode(g); n != NULL; n = nxtnode) {
		nxtnode = agnxtnode(g, n);

		labeln = agget(n, "label");
		labelnlen = strlen(labeln);

		for (e = agfstout(g, n); e != NULL; e = nxtout) {
			nxtout = agnxtout(g, e);

			labelm = agget(aghead(e), "label");
			labelmlen = strlen(labelm);
			value = agget(e, "value");

			printf("    {\"source\": \"%.*s\", \"target\": \"%.*s\", \"value\": \"%s\"}",
			    labelnlen > 2 ? labelnlen - 2 : 0,
			    labeln + 1,
			    labelmlen > 2 ? labelmlen - 2 : 0,
			    labelm + 1,
			    value != NULL ? value : "");
			if (nxtout != NULL || nxtnode != NULL)
				printf(",");
			printf("\n");
		}
	}
	printf("  ]\n");

	printf("}\n");
}

int
main(int argc, char *argv[])
{
	GVC_t *gvc;
	Agraph_t *g;
	char *gvargs[3];
	int ch;
	node_type_t nodetype;
	format_type_t formattype;

	nodetype = NODE_TYPE_COMPARTMENT;
	formattype = FORMAT_TYPE_D3_ARC;

	while ((ch = getopt(argc, argv, "f:n:")) != -1) {
		switch (ch) {
		case 'n':
			nodetype = node_type_from_str(optarg);
			if (nodetype == NODE_TYPE_INVALID)
				usage();
			break;
		case 'f':
			formattype = format_type_from_str(optarg);
			if (formattype == FORMAT_TYPE_INVALID)
				usage();
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	/* Set the layout engine to dot. */
	gvargs[0] = "dot";
	/* Parse the input file. */
	gvargs[1] = argv[0];
	gvargs[2] = NULL;

	gvc = gvContext();
	gvParseArgs(gvc, nitems(gvargs) - 1, gvargs);

	while ((g = gvNextInputGraph(gvc))) {
		graph_remove_intra_edges(g);
		switch (nodetype) {
		case NODE_TYPE_COMPARTMENT:
			g = graph_merge_intra_nodes(g);
			break;
		default:
			/* Do nothing. */
			break;
		}
		switch (formattype) {
		case FORMAT_TYPE_D3_ARC:
			graph_print_d3_arc(g);
			break;
		default:
			/* Do nothing. */
			break;
		}
	}

	(void)gvFreeContext(gvc);

	return (0);
}
