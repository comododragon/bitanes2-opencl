/* ********************************************************************************************* */
/* * Simple library for undirected unweighted graphs: libgraph                                 * */
/* * Author: André Bannwart Perina                                                             * */
/* ********************************************************************************************* */
/* * Copyright (c) 2017 André B. Perina                                                        * */
/* *                                                                                           * */
/* * libgraph is free software: you can redistribute it and/or modify it under the terms of    * */
/* * the GNU General Public License as published by the Free Software Foundation, either       * */
/* * version 3 of the License, or (at your option) any later version.                          * */
/* *                                                                                           * */
/* * libgraph is distributed in the hope that it will be useful, but WITHOUT ANY               * */
/* * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A           * */
/* * PARTICULAR PURPOSE.  See the GNU General Public License for more details.                 * */
/* *                                                                                           * */
/* * You should have received a copy of the GNU General Public License along with libgraph.    * */
/* * If not, see <http://www.gnu.org/licenses/>.                                               * */
/* ********************************************************************************************* */

#include "graph.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a graph with unconnected nodes.
 */
void graph_create(graph_t **graph, unsigned int n, unsigned int m) {
	int i;

	if(!graph)
		return;

	*graph = malloc(sizeof(graph_t));
	(*graph)->n = n;
	(*graph)->m = m;
	(*graph)->adj = malloc(n * sizeof(int *));

	(*graph)->raw = malloc(n * (n + 1) * sizeof(int));
	for(i = 0; i < n; i++) {
		(*graph)->adj[i] = &((*graph)->raw)[i * (n + 1)];
		(*graph)->adj[i][0] = 0;
	}
}

/**
 * @brief Connect two nodes.
 */
void graph_putEdge(graph_t *graph, unsigned int orig, unsigned int dest) {
	if(graph) {
		/* Add dest to the adjacency list and increase the number of filled elements */
		graph->adj[orig][graph->adj[orig][0] + 1] = dest;
		(graph->adj[orig][0])++;
	}
}

/**
 * @brief Check if two nodes are connected.
 */
int graph_getEdge(graph_t *graph, unsigned int orig, unsigned int dest) {
	int i;

	/* Iterate through the orig's adjacency list trying to find if the dest node is present */
	if(graph && graph->adj[orig]) {
		for(i = 0; i < graph->adj[orig][0]; i++) {
			if(dest == graph->adj[orig][i + 1])
				return 1;
		}

		return 0;
	}
	else {
		return 0;
	}
}

/**
 * @brief Get the adjacency list for a given node.
 */
int *graph_getAdjacents(graph_t *graph, unsigned int orig, unsigned int *noOfAdjacents) {
	/* Graph exists and orig node has adjacents */
	if(graph && graph->adj[orig]) {
		*noOfAdjacents = graph->adj[orig][0];
		/* Return the list starting from address 2 (since 0 and 1 are metadata) */
		return &(graph->adj[orig][1]);
	}

	*noOfAdjacents = 0;
	return NULL;
}

/**
 * @brief Destroy a graph; free memory.
 */
void graph_destroy(graph_t **graph) {
	int i;

	if(graph && *graph) {
		free((*graph)->raw);
		free((*graph)->adj);

		free(*graph);
	}
}

int *graph_compact(graph_t *graph, unsigned int **offsets) {
	if(graph && graph->raw) {
		*offsets = malloc(graph->n * sizeof(unsigned int));
		int *retVal = malloc(2 * graph->m * sizeof(int));
		int curOffset = 0;
		int i;

		for(i = 0; i < graph->n; i++) {
			memcpy(&retVal[curOffset], &(graph->adj[i])[1], (graph->adj)[i][0] * sizeof(int));
			(*offsets)[i] = curOffset;
			curOffset += (graph->adj)[i][0];
		}

		return retVal;
	}

	*offsets = NULL;
	return NULL;
}
