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

#ifndef GRAPH_H
#define GRAPH_H

/* Simple storage for a graph */
typedef struct {
	/* Number of nodes and edges in the graph */
	int n, m;
	/* Linearised adjacency list */
	int *raw;
	/* Adjacency list */
	int **adj;
} graph_t;

/**
 * @brief Create a graph with unconnected nodes.
 * @param graph Pointer to a graph_t pointer.
 * @param n Number of nodes.
 * @param m Number of edges (used only when adjacency list is used).
 */
void graph_create(graph_t **graph, unsigned int n, unsigned int m);

/**
 * @brief Connect two nodes.
 * @param graph Pointer to a graph_t structure.
 * @param orig Origin node.
 * @param dest Destination node.
 */
void graph_putEdge(graph_t *graph, unsigned int orig, unsigned int dest);

/**
 * @brief Check if two nodes are connected.
 * @param graph Pointer to a graph_t structure.
 * @param orig Origin node.
 * @param dest Destination node.
 * @return 1 if these nodes are connected, 0 otherwise.
 */
int graph_getEdge(graph_t *graph, unsigned int orig, unsigned int dest);

/**
 * @brief Get the adjacency list for a given node.
 * @param graph Pointer to a graph_t structure.
 * @param orig Origin node.
 * @param noOfAdjacents Reference to a int variable where the number of adjacents will be assigned.
 * @return An array of ints containing all connected nodes to orig. NULL will be returned if
 *         the graph does not exist or if the node has no adjacents.
 * @note This function only exists if adjacency matrix mode is not used (GRAPH_USE_ADJ_MATRIX macro not set).
 */
int *graph_getAdjacents(graph_t *graph, unsigned int orig, unsigned int *noOfAdjacents);

/**
 * @brief Destroy a graph; free memory.
 * @param graph Pointer to a pointer of a graph_t structure.
 */
void graph_destroy(graph_t **graph);

int *graph_compact(graph_t *graph, unsigned int **offsets);

#endif
