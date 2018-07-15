/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <rpearce@llnl.gov>.
 * LLNL-CODE-644630.
 * All rights reserved.
 *
 * This file is part of HavoqGT, Version 0.1.
 * For details, see
 * https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Our Notice and GNU Lesser General Public
 * License.
 *   http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR
 * A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
 *
 * Our Preamble Notice
 *
 * A. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at the Lawrence
 * Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with the
 * DOE.
 *
 * B. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or
 * process
 * disclosed, or represents that its use would not infringe privately-owned
 * rights.
 *
 * C. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring
 * by
 * the United States Government or Lawrence Livermore National Security, LLC.
 * The
 * views and opinions of authors expressed herein do not necessarily state or
 * reflect those of the United States Government or Lawrence Livermore National
 * Security, LLC, and shall not be used for advertising or product endorsement
 * purposes.
 *
 */
#pragma include once

#include <boost/container/deque.hpp>
#include <fstream>
#include <havoqgt/visitor_queue.hpp>
//#include <boost/container/flat_map.hpp>
#include <map>
#include <unordered_map>

namespace havoqgt {

struct dogr_edge {
  uint32_t target_degree       = 0;
  uint32_t edge_triangle_count = 0;
};

template <typename Visitor>
class lifo_queue {
 protected:
  std::vector<Visitor> m_data;

 public:
  lifo_queue() {}

  bool push(Visitor const& task) {
    m_data.push_back(task);
    return true;
  }

  void pop() { m_data.pop_back(); }

  Visitor const& top()  // const
  {
    return m_data.back();
  }

  size_t size() const {
    return m_data.size();
    ;
  }

  bool empty() const { return m_data.empty(); }

  void clear() { m_data.clear(); }
};

template <typename Graph>
class vis_dod_in_degree {
 public:
  typedef vis_dod_in_degree<Graph>       my_type;
  typedef typename Graph::vertex_locator vertex_locator;

  vis_dod_in_degree() : vertex() {}

  vis_dod_in_degree(vertex_locator v) : vertex(v) {}
  template <typename AlgData>
  bool pre_visit(AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (!vertex.is_delegate_master()) {
        return true;
      }
    }
    std::get<1>(alg_data)[vertex]++;
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool init_visit(Graph& g, VisitorQueueHandle vis_queue,
                  AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (!vertex.is_delegate_master()) {
        std::cerr << "vis_dod_in_degree() -- delegate slaves shouldn't be here"
                  << std::endl;
        exit(-1);
      }
    }
    for (auto& dodedge : std::get<0>(alg_data)[vertex]) {
      vertex_locator neighbor = dodedge.first;
      my_type        new_visitor(neighbor);
      vis_queue->queue_visitor(new_visitor);
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool visit(Graph& g, VisitorQueueHandle vis_queue, AlgData& alg_data) const {
    std::cerr << "vis_dod_in_degree() -- no visit()" << std::endl;
    exit(-1);
  }

  vertex_locator vertex;
};

template <typename Graph>
class vis_dod_round {
 public:
  typedef vis_dod_round<Graph>           my_type;
  typedef typename Graph::vertex_locator vertex_locator;

  vis_dod_round() : vertex() {}
  vis_dod_round(vertex_locator v) : vertex(v), round(0) {}
  vis_dod_round(vertex_locator v, uint32_t r) : vertex(v), round(r) {}

  template <typename AlgData>
  bool pre_visit(AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (!vertex.is_delegate_master()) {
        return true;
      }
    }
    if (std::get<1>(alg_data)[vertex] == 0) {
      std::cerr << "vis_dod_round counting error" << std::endl;
    }
    std::get<2>(alg_data)[vertex] =
        std::max(round, std::get<2>(alg_data)[vertex]);
    if (--std::get<1>(alg_data)[vertex] == 0) {
      // all in-edges have reported in, time to set my round
      std::get<2>(alg_data)[vertex]++;
      return true;
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool init_visit(Graph& g, VisitorQueueHandle vis_queue,
                  AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (!vertex.is_delegate_master()) {
        std::cerr << "vis_dod_in_degree() -- delegate slaves shouldn't be here"
                  << std::endl;
        exit(-1);
      }
    }
    if (std::get<1>(alg_data)[vertex] != 0) return false;
    if (std::get<2>(alg_data)[vertex] != 0) return false;
    std::get<2>(alg_data)[vertex] = 1;
    for (auto& dodedge : std::get<0>(alg_data)[vertex]) {
      vertex_locator neighbor = dodedge.first;
      my_type        new_visitor(neighbor, 1);
      vis_queue->queue_visitor(new_visitor);
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool visit(Graph& g, VisitorQueueHandle vis_queue, AlgData& alg_data) const {
    if (std::get<1>(alg_data)[vertex] != 0) {
      std::cerr << "vis_dod_round visit() -- not all in edges reported"
                << std::endl;
    }
    for (auto& dodedge : std::get<0>(alg_data)[vertex]) {
      vertex_locator neighbor = dodedge.first;
      my_type        new_visitor(neighbor, std::get<2>(alg_data)[vertex]);
      vis_queue->queue_visitor(new_visitor);
    }
    return false;
  }

  vertex_locator vertex;
  uint32_t       round;
};

template <typename Graph>
class core2_visitor {
 public:
  typedef core2_visitor<Graph>           my_type;
  typedef typename Graph::vertex_locator vertex_locator;

  core2_visitor() : vertex(), init(true) {}

  core2_visitor(vertex_locator v) : vertex(v), init(true) {}

  core2_visitor(vertex_locator v, bool _init) : vertex(v), init(_init) {}

  template <typename AlgData>
  bool pre_visit(AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (!vertex.is_delegate_master()) {
        return true;
      }
    }
    if (std::get<1>(alg_data)[vertex]) {
      --(std::get<0>(alg_data)[vertex]);

      if (std::get<0>(alg_data)[vertex] < 2) {
        // remove from 2 core
        std::get<1>(alg_data)[vertex] = false;
        std::get<0>(alg_data)[vertex] = 0;
        return true;
      }
    }

    // Retrun true if alive
    // return std::get<1>(alg_data)[vertex];
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool init_visit(Graph& g, VisitorQueueHandle vis_queue,
                  AlgData& alg_data) const {
    if (std::get<1>(alg_data)[vertex]) {
      //   --(std::get<0>(alg_data)[vertex]);
      if (std::get<0>(alg_data)[vertex] < 2) {
        // remove from 2 core
        std::get<1>(alg_data)[vertex] = false;
        std::get<0>(alg_data)[vertex] = 0;
        for (auto eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex);
             ++eitr) {
          vertex_locator neighbor = eitr.target();
          my_type        new_visitor(neighbor, false);
          vis_queue->queue_visitor(new_visitor);
        }
      }
      return true;
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool visit(Graph& g, VisitorQueueHandle vis_queue, AlgData& alg_data) const {
    if (init) {
      if (std::get<0>(alg_data)[vertex] < 2) {
        // remove from 2 core
        std::get<1>(alg_data)[vertex] = false;
        std::get<0>(alg_data)[vertex] = 0;
        for (auto eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex);
             ++eitr) {
          vertex_locator neighbor = eitr.target();
          my_type        new_visitor(neighbor, false);
          vis_queue->queue_visitor(new_visitor);
        }
      }
      return true;
    }

    if (std::get<1>(alg_data)[vertex]) {
      std::cerr << "LOGIC ERROR" << std::endl;
      exit(-1);
    }
    // if(std::get<1>(alg_data)[vertex]) {
    //  --(std::get<0>(alg_data)[vertex]);
    //  if(std::get<0>(alg_data)[vertex] < 2) {
    //    //remove from 2 core
    //    std::get<1>(alg_data)[vertex] = false;
    //    std::get<0>(alg_data)[vertex] = 0;
    for (auto eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex);
         ++eitr) {
      vertex_locator neighbor = eitr.target();
      my_type        new_visitor(neighbor, false);
      vis_queue->queue_visitor(new_visitor);
    }
    //  }
    //  return true;
    //}
    // return false;
    return true;
  }

  friend inline bool operator>(const core2_visitor& v1,
                               const core2_visitor& v2) {
    return false;
  }

  friend inline bool operator<(const core2_visitor& v1,
                               const core2_visitor& v2) {
    return false;
  }

  vertex_locator vertex;
  bool           init;
};

template <typename Graph>
class directed_core2 {
 public:
  typedef directed_core2<Graph>          my_type;
  typedef typename Graph::vertex_locator vertex_locator;

  directed_core2() : vertex(), init(true) {}

  directed_core2(vertex_locator v) : vertex(v), init(true) {}

  directed_core2(vertex_locator v, vertex_locator _from, uint32_t _from_degree)
      : vertex(v), from_label(_from), from_degree(_from_degree), init(false) {}

  template <typename AlgData>
  bool pre_visit(AlgData& alg_data) const {
    // if(std::get<0>(alg_data)[vertex] >= 2) {
    if (from_degree >= std::get<2>(alg_data).degree(
                           vertex) /*std::get<0>(alg_data)[vertex]*/) {
      // previously returned true, but changing here --- return true;
      if (vertex.is_delegate()) {
        if (!vertex.is_delegate_master()) {
          return true;
        }
      }
      if (std::get<0>(alg_data)[vertex] < 2) return false;
      // only here should be low-degree & masters
      if (from_degree > std::get<2>(alg_data).degree(
                            vertex) /*std::get<0>(alg_data)[vertex]*/
          || (from_degree ==
                  std::get<2>(alg_data).degree(
                      vertex) /*std::get<0>(alg_data)[vertex]*/
              && vertex < from_label)) {
        auto vv = vertex;
        vv.set_bcast(0);
        vv.set_intercept(0);
        auto fl = from_label;
        fl.set_bcast(0);
        fl.set_intercept(0);

        std::get<1>(alg_data)[vv][fl].target_degree = from_degree;
        // std::get<1>(alg_data)[vv].add(fl, from_degree);
      }
    }
    //}
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool init_visit(Graph& g, VisitorQueueHandle vis_queue,
                  AlgData& alg_data) const {
    if (std::get<0>(alg_data)[vertex] >= 2) {
      // if in 2core, send degree to neighbors
      uint32_t my_degree = /*std::get<0>(alg_data)[vertex];*/ g.degree(vertex);
      for (auto eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex);
           ++eitr) {
        vertex_locator neighbor = eitr.target();
        if (neighbor == vertex) continue;
        my_type new_visitor(neighbor, vertex, my_degree);
        vis_queue->queue_visitor(new_visitor);
      }
      return true;
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool visit(Graph& g, VisitorQueueHandle vis_queue, AlgData& alg_data) const {
    if (init) {
      if (std::get<0>(alg_data)[vertex] >= 2) {
        // if in 2core, send degree to neighbors
        uint32_t my_degree =
            /*std::get<0>(alg_data)[vertex];*/ g.degree(vertex);
        for (auto eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex);
             ++eitr) {
          vertex_locator neighbor = eitr.target();
          if (neighbor == vertex) continue;
          my_type new_visitor(neighbor, vertex, my_degree);
          vis_queue->queue_visitor(new_visitor);
        }
        return true;
      }
    }

    //    if(from_degree > g.degree(vertex) /*std::get<0>(alg_data)[vertex]*/ ||
    //      (from_degree == g.degree(vertex)/*std::get<0>(alg_data)[vertex]*/ &&
    //      vertex < from_label )) {
    //
    //      auto vv = vertex;
    //      vv.set_bcast(0);
    //      vv.set_intercept(0);
    //      auto fl = from_label;
    //      fl.set_bcast(0);
    //      fl.set_intercept(0);
    //
    //      std::get<1>(alg_data)[vv][fl] = from_degree;
    //    }
    return false;
  }

  friend inline bool operator>(const directed_core2& v1,
                               const directed_core2& v2) {
    return false;
  }

  friend inline bool operator<(const directed_core2& v1,
                               const directed_core2& v2) {
    return false;
  }

  vertex_locator vertex;
  vertex_locator from_label;
  uint32_t       from_degree;
  bool           init;
};

template <typename Graph>
class core2_wedges {
 public:
  typedef core2_wedges<Graph>            my_type;
  typedef typename Graph::vertex_locator vertex_locator;

  core2_wedges() : vertex() {}

  core2_wedges(vertex_locator v) : vertex(v) {}

  core2_wedges(vertex_locator v, vertex_locator _check_close,
               vertex_locator _from_vertex, bool _do_check_close)
      : vertex(v),
        check_close(_check_close),
        from_vertex(_from_vertex),
        do_check_close(_do_check_close) {}

  template <typename AlgData>
  bool pre_visit(AlgData& alg_data) const {
    if (vertex.is_delegate()) {
      if (vertex.is_delegate_master()) {
        if (do_check_close) {
          ++std::get<2>(alg_data);
          // std::get<3>(alg_data) <<
          // std::get<4>(alg_data).locator_to_label(vertex) << " " <<
          // check_close
          // << std::endl;
          // std::get<4>(alg_data)[vertex]++;
          if (std::get<0>(alg_data)[vertex].count(check_close) > 0) {
            ++std::get<1>(alg_data);
            // std::get<5>(alg_data)[vertex]++;
            // std::cout << "FOUND" << std::endl;
            std::get<0>(alg_data)[vertex][check_close].edge_triangle_count++;
            return true;  /// now returning true to count
          }
        } else {  // already found, not counting
          if (std::get<0>(alg_data)[vertex].count(check_close) == 0 ||
              std::get<0>(alg_data)[vertex].count(from_vertex) == 0) {
            std::cerr << "Error in edge counting" << std::endl;
          } else {
            std::get<0>(alg_data)[vertex][check_close].edge_triangle_count++;
            std::get<0>(alg_data)[vertex][from_vertex].edge_triangle_count++;
          }
        }
        return false;
      }
      return true;
    }
    if (do_check_close) {
      ++std::get<2>(alg_data);
      // std::get<4>(alg_data)[vertex]++;
      // std::get<3>(alg_data) << std::get<4>(alg_data).locator_to_label(vertex)
      // << " " << check_close << std::endl;
      if (std::get<0>(alg_data)[vertex].count(check_close) > 0) {
        ++std::get<1>(alg_data);
        // std::get<5>(alg_data)[vertex]++;
        // std::cout << "FOUND" << std::endl;
        std::get<0>(alg_data)[vertex][check_close].edge_triangle_count++;
        return true;
      }
    } else {  // already found, not counting
      if (std::get<0>(alg_data)[vertex].count(check_close) == 0 ||
          std::get<0>(alg_data)[vertex].count(from_vertex) == 0) {
        std::cerr << "Error in edge counting" << std::endl;
      } else {
        std::get<0>(alg_data)[vertex][check_close].edge_triangle_count++;
        std::get<0>(alg_data)[vertex][from_vertex].edge_triangle_count++;
      }
    }
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool init_visit(Graph& g, VisitorQueueHandle vis_queue,
                  AlgData& alg_data) const {
    // if (std::get<3>(alg_data)[vertex] <= std::get<4>(alg_data)) {
    if (std::get<0>(alg_data)[vertex].size() > 1) {
      for (const auto& pair_a : std::get<0>(alg_data)[vertex]) {
        for (const auto& pair_b : std::get<0>(alg_data)[vertex]) {
          if (pair_a.second.target_degree < pair_b.second.target_degree ||
              (pair_a.second.target_degree == pair_b.second.target_degree &&
               pair_a.first < pair_b.first)) {
            my_type new_visitor(pair_a.first, pair_b.first, vertex, true);
            vis_queue->queue_visitor(new_visitor);
            // std::get<3>(alg_data)[vertex]++;
            // fake ++std::get<1>(alg_data);  //fake counting here
          }
        }
      }
      /*for(auto itr_i = std::get<0>(alg_data)[vertex].begin(); itr_i !=
      std::get<0>(alg_data)[vertex].end(); ++itr_i) {
        for(auto itr_j = itr_i+1; itr_j !=
      std::get<0>(alg_data)[vertex].end();
      ++itr_j) {
          auto pair_a = *itr_i;
          auto pair_b = *itr_j;
          if(pair_a.second < pair_b.second ||
            (pair_a.second == pair_b.second && pair_a.first < pair_b.first)) {
              my_type new_visitor(g.label_to_locator(pair_a.first),
      pair_b.first);
              //vis_queue->queue_visitor(new_visitor);
              ++std::get<1>(alg_data);  //fake counting here
          } else {
            my_type new_visitor(g.label_to_locator(pair_b.first),
      pair_a.first);
            //vis_queue->queue_visitor(new_visitor);
             ++std::get<1>(alg_data);  //fake counting here
          }
        }
      }*/

      //        ++std::get<1>(alg_data);  //fake counting here
    }
    //}
    return false;
  }

  template <typename VisitorQueueHandle, typename AlgData>
  bool visit(Graph& g, VisitorQueueHandle vis_queue, AlgData& alg_data) const {
    if (do_check_close) {
      my_type new_visitor(from_vertex, check_close, vertex, false);
      vis_queue->queue_visitor(new_visitor);
      return false;
    }
    std::cout << "Shoudn't be here" << std::endl;
    exit(-1);
    /*    ++std::get<2>(alg_data);
        std::get<3>(alg_data) << g.locator_to_label(vertex) << " " <<
       check_close << std::endl;
        if(std::get<0>(alg_data)[vertex].count(check_close) > 0) {
          ++std::get<1>(alg_data);
          //std::cout << "FOUND" << std::endl;
        }
        return false;*/
  }

  friend inline bool operator>(const core2_wedges& v1, const core2_wedges& v2) {
    return false;
  }

  friend inline bool operator<(const core2_wedges& v1, const core2_wedges& v2) {
    return false;
  }

  vertex_locator vertex;
  vertex_locator check_close;
  vertex_locator from_vertex;
  bool           do_check_close;
};

void output_degree_distribution(
    const std::map<uint64_t, uint64_t>& local_degree_count, const char* fname) {
  int mpi_size(0), mpi_rank(0);
  CHK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank));
  CHK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &mpi_size));

  std::vector<std::vector<std::pair<uint64_t, uint64_t>>> send_p_vec(mpi_size);
  std::vector<std::vector<std::pair<uint64_t, uint64_t>>> recv_p_vec(mpi_size);
  for (auto deg_cnt : local_degree_count) {
    send_p_vec[deg_cnt.first % uint64_t(mpi_size)].push_back(deg_cnt);
  }

  mpi_all_to_all(send_p_vec, recv_p_vec, MPI_COMM_WORLD);

  std::map<uint64_t, uint64_t> partitioned_deg_count;
  for (auto& vec_deg_cnt : recv_p_vec) {
    for (auto deg_cnt : vec_deg_cnt) {
      partitioned_deg_count[deg_cnt.first] += deg_cnt.second;
    }
  }

  //
  // Send all to Rank 0 -- this is not efficient way
  send_p_vec.clear();
  send_p_vec.resize(mpi_size);
  recv_p_vec.clear();
  recv_p_vec.resize(mpi_size);
  for (auto vec_deg_cnt : partitioned_deg_count) {
    send_p_vec[0].push_back(vec_deg_cnt);
  }
  mpi_all_to_all(send_p_vec, recv_p_vec, MPI_COMM_WORLD);

  if (mpi_rank == 0) {
    std::vector<std::pair<uint64_t, uint64_t>> all_sorted;
    for (auto& vec_deg_cnt : recv_p_vec) {
      for (auto deg_cnt : vec_deg_cnt) {
        all_sorted.push_back(deg_cnt);
      }
    }
    std::sort(all_sorted.begin(), all_sorted.end());
    std::ofstream outfile(fname);
    for (auto deg_cnt : all_sorted) {
      outfile << deg_cnt.first << "\t" << deg_cnt.second << std::endl;
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
}
struct vertex_locator_hash {
  template <typename T>
  std::size_t operator()(const T& k) const {
    return k.hash();
  }
};

template <typename TGraph>
uint64_t ktruss_round(TGraph& g) {
  typedef TGraph                          graph_type;
  typedef typename TGraph::vertex_locator vertex_locator;

  int mpi_size(0), mpi_rank(0);
  CHK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank));
  CHK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &mpi_size));

  // std::stringstream fname_sv_partition, fname_wc_partition;
  // fname_sv_partition << "/p/lscratchf/havoqgtu/wedge_check_experiment/DOD_"
  // << mpi_rank << "_of_" << mpi_size;
  // fname_wc_partition <<
  // "/p/lscratchf/havoqgtu/wedge_check_experiment/wedges_" << mpi_rank <<
  // "_of_" << mpi_size;
  //
  // std::ofstream ofs_sv_partition(fname_sv_partition.str().c_str());
  // std::ofstream ofs_wc_partition(fname_wc_partition.str().c_str());

  //
  // 1)  Calculate 2core degree.   0 degree means that vertex is not in 2core
  typename graph_type::template vertex_data<
      std::map<vertex_locator, dogr_edge>,
      std::allocator<std::map<vertex_locator, dogr_edge>>>
      core2_directed(g);
  {
    typename graph_type::template vertex_data<uint32_t,
                                              std::allocator<uint32_t>>
        core2_degree(g);
    {
      typename graph_type::template vertex_data<bool, std::allocator<bool>>
          core2_alive(g);
      core2_alive.reset(true);
      for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
        core2_degree[*vitr] = g.degree(*vitr);
      }
      for (auto ditr = g.delegate_vertices_begin();
           ditr != g.delegate_vertices_end(); ++ditr) {
        core2_degree[*ditr] = g.degree(*ditr);
      }

      // /// This computes the 2core
      double start_time = MPI_Wtime();
      {
        auto alg_data = std::forward_as_tuple(core2_degree, core2_alive);
        auto vq = create_visitor_queue<core2_visitor<graph_type>, lifo_queue>(
            &g, alg_data);
        vq.init_visitor_traversal();
      }
      double end_time = MPI_Wtime();
      if (mpi_rank == 0) {
        std::cout << "2Core time = " << end_time - start_time << std::endl;
      }

      // for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr)
      // {
      //   std::cout << "Vertex " << g.locator_to_label(*vitr) << " has
      //   core2_degree = " << core2_degree[*vitr] << std::endl;
      // }
      // for (auto ditr = g.delegate_vertices_begin(); ditr !=
      // g.delegate_vertices_end(); ++ditr) {
      //   std::cout << "Delegate " << g.locator_to_label(*ditr) << " has
      //   core2_degree = " <<  core2_degree[*ditr] << std::endl;
      // }
    }

    //
    // 2)  Calculate directed 2core edges
    // typename graph_type::template
    // vertex_data<boost::container::flat_map<vertex_locator,uint32_t>,
    // std::allocator<boost::container::flat_map<vertex_locator,uint32_t>> >
    // core2_directed(g);
    // typename graph_type::template
    // vertex_data<std::unordered_map<vertex_locator,uint32_t,vertex_locator_hash>,
    // std::allocator<std::unordered_map<vertex_locator,uint32_t,vertex_locator_hash>
    // > >   core2_directed(g);
    double start_time = MPI_Wtime();
    {
      auto alg_data = std::forward_as_tuple(core2_degree, core2_directed, g);
      auto vq = create_visitor_queue<directed_core2<graph_type>, lifo_queue>(
          &g, alg_data);
      vq.init_visitor_traversal();
    }
    double end_time = MPI_Wtime();
    if (mpi_rank == 0) {
      std::cout << "Directed 2Core time = " << end_time - start_time
                << std::endl;
    }

    uint64_t local_core2_directed_edge_count(0);
    for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
      local_core2_directed_edge_count += core2_directed[*vitr].size();
    }

    uint64_t global_core2_directed_edge_count =
        comm_world().all_reduce(local_core2_directed_edge_count, MPI_SUM);
    if (comm_world().rank() == 0) {
      std::cout << "global_core2_directed_edge_count = "
                << global_core2_directed_edge_count << std::endl;
    }

    // for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
    //    ofs_sv_partition << g.locator_to_label(*vitr);
    //    for(auto pair : core2_directed[*vitr]) {
    //      ofs_sv_partition << " " << pair.first;
    //    }
    //    ofs_sv_partition << std::endl;
    //}

    // for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
    //   std::cout << "Vertex " << g.locator_to_label(*vitr) << " :: ";
    //     for(auto p : core2_directed[*vitr]) {
    //       std::cout << p.first << " ";
    //     }
    //     std::cout << std::endl;
    // }

    // (was) Sort Directed 2 core
    MPI_Barrier(MPI_COMM_WORLD);
    uint64_t local_max_dod(0), local_max_deg(0);
    start_time = MPI_Wtime();
    {
      for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
        local_max_dod =
            std::max(local_max_dod, uint64_t(core2_directed[*vitr].size()));
        local_max_deg = std::max(local_max_deg, uint64_t(g.degree(*vitr)));
      }
      for (auto citr = g.controller_begin(); citr != g.controller_end();
           ++citr) {
        local_max_dod =
            std::max(local_max_dod, uint64_t(core2_directed[*citr].size()));
        local_max_deg = std::max(local_max_deg, uint64_t(g.degree(*citr)));
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();
    uint64_t global_max_dod =
        mpi_all_reduce(local_max_dod, std::greater<uint64_t>(), MPI_COMM_WORLD);
    uint64_t global_max_deg =
        mpi_all_reduce(local_max_deg, std::greater<uint64_t>(), MPI_COMM_WORLD);
    if (mpi_rank == 0) {
      std::cout << "Largest DOD out degree = " << global_max_dod << std::endl;
      std::cout << "Largest orig degree = " << global_max_deg << std::endl;
    }
    {
      //
      // 4)  Compute distributions
      std::map<uint64_t, uint64_t> local_orig_degree;
      std::map<uint64_t, uint64_t> local_2core_degree;
      std::map<uint64_t, uint64_t> local_2core_out_degree;

      uint64_t local_edge_count(0), local_dod_edge_count(0),
          local_in_zero_count(0), local_in_zero_edges_count(0);

      for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
        local_orig_degree[g.degree(*vitr)]++;
        local_2core_degree[core2_degree[*vitr]]++;
        local_2core_out_degree[core2_directed[*vitr].size()]++;
        local_edge_count += g.degree(*vitr);
        local_dod_edge_count += core2_directed[*vitr].size();
        if (core2_degree[*vitr] == core2_directed[*vitr].size()) {
          ++local_in_zero_count;
          local_in_zero_edges_count += core2_directed[*vitr].size();
        }
      }
      for (auto citr = g.controller_begin(); citr != g.controller_end();
           ++citr) {
        local_orig_degree[g.degree(*citr)]++;
        local_2core_degree[core2_degree[*citr]]++;
        local_2core_out_degree[core2_directed[*citr].size()]++;
        local_edge_count += g.degree(*citr);
        local_dod_edge_count += core2_directed[*citr].size();
        if (core2_degree[*citr] == core2_directed[*citr].size()) {
          ++local_in_zero_count;
          local_in_zero_edges_count += core2_directed[*citr].size();
        }
      }

      uint64_t global_edge_count = mpi_all_reduce(
          local_edge_count, std::plus<uint64_t>(), MPI_COMM_WORLD);
      uint64_t global_dod_edge_count = mpi_all_reduce(
          local_dod_edge_count, std::plus<uint64_t>(), MPI_COMM_WORLD);
      uint64_t global_in_zero_count = mpi_all_reduce(
          local_in_zero_count, std::plus<uint64_t>(), MPI_COMM_WORLD);
      uint64_t global_in_zero_edge_count = mpi_all_reduce(
          local_in_zero_edges_count, std::plus<uint64_t>(), MPI_COMM_WORLD);

      if (mpi_rank == 0) {
        std::cout << "global_edge_count = " << global_edge_count << std::endl;
        std::cout << "global_dod_edge_count = " << global_dod_edge_count
                  << std::endl;
        std::cout << "global_in_zero_count = " << global_in_zero_count
                  << std::endl;
        std::cout << "global_in_zero_edge_count = " << global_in_zero_edge_count
                  << std::endl;
      }

      // std::stringstream orig_degree, tcore_degree, tcore_out_degree;
      // orig_degree << deg_output_fname << "_orig_degree.txt";
      // tcore_degree << deg_output_fname << "_2core_degree.txt";
      // tcore_out_degree << deg_output_fname << "_2core_out_degree.txt";

      // output_degree_distribution(local_orig_degree,
      // orig_degree.str().c_str());
      // output_degree_distribution(local_2core_degree,
      // tcore_degree.str().c_str());
      // output_degree_distribution(local_2core_out_degree,
      // tcore_out_degree.str().c_str());
    }
  }

  // for (uint32_t round = 0; round < global_max_round + 1; ++round) {
  //   size_t global_round_size =
  //       comm_world().all_reduce(map_round_local_vertex[round].size(),
  //       MPI_SUM);
  //   if (comm_world().rank() == 0) {
  //     std::cout << "Round " << round << " size = " << global_round_size
  //               << std::endl;
  //   }
  // }
  // MPI_Abort(MPI_COMM_WORLD, -1);
  //
  // 3)  Build wedges & count
  double   total_ktruss_start_time = MPI_Wtime();
  uint64_t global_edges_remain(0);
  int      k = 3;
  do {
    double single_ktruss_start_time = MPI_Wtime();
    ///////  RECOMPUTE ROUND PER KCORE
    //
    //  Compute "round" information
    typename graph_type::template vertex_data<uint32_t,
                                              std::allocator<uint32_t>>
        dod_in_degree(g);
    dod_in_degree.reset(0);
    typename graph_type::template vertex_data<uint32_t,
                                              std::allocator<uint32_t>>
        dod_round(g);
    dod_round.reset(1);
    MPI_Barrier(MPI_COMM_WORLD);
    double dod_in_degree_start_time = MPI_Wtime();
    {
      auto alg_data = std::forward_as_tuple(core2_directed, dod_in_degree);
      auto vq = create_visitor_queue<vis_dod_in_degree<graph_type>, lifo_queue>(
          &g, alg_data);
      vq.init_visitor_traversal();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    double   dod_in_degree_end_time = MPI_Wtime();
    uint64_t local_count_zero_in_degree(0);
    for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
      if (dod_in_degree[*vitr] == 0) {
        ++local_count_zero_in_degree;
        dod_round[*vitr] = 0;
      }
    }
    for (auto vitr = g.controller_begin(); vitr != g.controller_end(); ++vitr) {
      if (dod_in_degree[*vitr] == 0) {
        ++local_count_zero_in_degree;
        dod_round[*vitr] = 0;
      }
    }
    uint64_t global_count_zero_in_degree =
        comm_world().all_reduce(local_count_zero_in_degree, MPI_SUM);

    MPI_Barrier(MPI_COMM_WORLD);
    double dod_round_start_time = MPI_Wtime();
    {
      auto alg_data =
          std::forward_as_tuple(core2_directed, dod_in_degree, dod_round);
      auto vq = create_visitor_queue<vis_dod_round<graph_type>, lifo_queue>(
          &g, alg_data);
      vq.init_visitor_traversal();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    double   dod_round_end_time = MPI_Wtime();
    uint32_t local_max_round(0);
    std::map<uint32_t, std::vector<vertex_locator>> map_round_local_vertex;
    for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
      --dod_round[*vitr];
      local_max_round = std::max(local_max_round, dod_round[*vitr]);
      if (core2_directed[*vitr].size() > 0) {  // skip zero out degree verts
        map_round_local_vertex[dod_round[*vitr]].push_back(*vitr);
      }
    }
    for (auto vitr = g.controller_begin(); vitr != g.controller_end(); ++vitr) {
      --dod_round[*vitr];
      local_max_round = std::max(local_max_round, dod_round[*vitr]);
      if (core2_directed[*vitr].size() > 0) {  // skip zero out degree verts
        map_round_local_vertex[dod_round[*vitr]].push_back(*vitr);
      }
    }
    uint32_t global_max_round =
        comm_world().all_reduce(local_max_round, MPI_MAX);
    if (comm_world().rank() == 0) {
      std::cout << "Starting ktruss k = " << k << std::endl;
      // std::cout << "dod_round_time = "
      // << dod_round_start_time - dod_round_end_time << std::endl;
      std::cout << "global_max_round = " << global_max_round << std::endl;
    }
    //
    // repack into log bins
    uint64_t num_bins = 0;
    {
      auto tmp_max_round = global_max_round;
      while (tmp_max_round >>= 1) ++num_bins;
    }
    std::vector<std::vector<vertex_locator>> log_bins(num_bins + 1);
    for (uint32_t old_round = 0; old_round <= global_max_round; ++old_round) {
      auto   tmp_old_round = old_round;
      size_t bin           = 0;
      while (tmp_old_round >>= 1) ++bin;
      for (auto& loc : map_round_local_vertex[old_round]) {
        log_bins[bin].push_back(loc);
      }
    }

    ///////  END COMPUTE ROUND PER KCORE
    uint64_t global_edges_deleted(0);
    uint64_t local_edges_remain(0);
    for (uint32_t round = 0; round < log_bins.size(); /*no incr*/) {
      auto& sources = log_bins[round];

      // if (comm_world().rank() == 0)
      //   std::cout << "Starting Round " << round << " of " << global_max_round
      //             << std::endl;

      uint64_t local_edges_deleted(0);
      uint64_t local_triangle_count(0), local_wedge_count(0);
      MPI_Barrier(MPI_COMM_WORLD);
      double start_time = MPI_Wtime();
      {
        auto alg_data = std::forward_as_tuple(
            core2_directed, local_triangle_count, local_wedge_count, dod_round,
            round /*, count_wedges_created, count_wedges_checked, count_triangles_found*/);
        auto vq = create_visitor_queue<core2_wedges<graph_type>, lifo_queue>(
            &g, alg_data);
        vq.init_visitor_traversal(sources);
      }
      MPI_Barrier(MPI_COMM_WORLD);
      double   end_time           = MPI_Wtime();
      uint64_t global_wedge_count = mpi_all_reduce(
          local_wedge_count, std::plus<uint64_t>(), MPI_COMM_WORLD);
      // if (mpi_rank == 0) {
      //   std::cout << "TC time = " << end_time - start_time
      //             << "Total wedges checked = " << global_wedge_count
      //             << std::endl;
      // }

      uint64_t local_edge_triangle_count(0);
      for (vertex_locator round_source : sources) {
        for (auto deitr = core2_directed[round_source].begin();
             deitr != core2_directed[round_source].end();
             /* no incr*/) {
          if (deitr->second.edge_triangle_count >= k - 2) {
            // deitr->second.edge_triangle_count = 0;  // reset for next count
            ++local_edges_remain;
            ++deitr;
          } else {
            ++local_edges_deleted;
            deitr = core2_directed[round_source].erase(deitr);  // remove
          }
        }
      }
      global_edges_deleted +=
          comm_world().all_reduce(local_edges_deleted, MPI_SUM);
      if (global_edges_deleted == 0 ||
          (global_edges_deleted < global_edges_remain * 0.01 &&
           round < log_bins.size() - 1)) {
        // allow to go to next round
        ++round;
      } else {
        // clear and start all over :(
        // erase for next count
        local_edges_remain = 0;

        for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
          for (auto& dode : core2_directed[*vitr]) {
            dode.second.edge_triangle_count = 0;
          }
        }
        for (auto citr = g.controller_begin(); citr != g.controller_end();
             ++citr) {
          for (auto& dode : core2_directed[*citr]) {
            dode.second.edge_triangle_count = 0;
          }
        }
        if (comm_world().rank() == 0) {
          std::cout << "Restarting -- Deleted " << global_edges_deleted
                    << " edges"
                    << " round " << round << " of " << log_bins.size()
                    << std::endl;
        }
        global_edges_deleted = 0;
        round                = 0;
      }
    }
    global_edges_remain = comm_world().all_reduce(local_edges_remain, MPI_SUM);
    double single_ktruss_end_time = MPI_Wtime();

    if (comm_world().rank() == 0) {
      std::cout << "K = " << k
                << "   global_edges_remain = " << global_edges_remain
                << " TIME = "
                << single_ktruss_end_time - single_ktruss_start_time
                << std::endl;
    }
    ++k;  // compute next k.
          //
          // precut here for ++K and clear
          // erase for next count
    local_edges_remain = 0;
    for (auto vitr = g.vertices_begin(); vitr != g.vertices_end(); ++vitr) {
      for (auto deitr = core2_directed[*vitr].begin();
           deitr != core2_directed[*vitr].end();
           /* no incr*/) {
        if (deitr->second.edge_triangle_count >= k - 2) {
          deitr->second.edge_triangle_count = 0;  // reset for next count
          ++local_edges_remain;
          ++deitr;
        } else {
          deitr = core2_directed[*vitr].erase(deitr);  // remove
        }
      }
    }
    for (auto citr = g.controller_begin(); citr != g.controller_end(); ++citr) {
      for (auto deitr = core2_directed[*citr].begin();
           deitr != core2_directed[*citr].end();
           /* no incr*/) {
        if (deitr->second.edge_triangle_count >= k - 2) {
          deitr->second.edge_triangle_count = 0;  // reset for next count
          ++local_edges_remain;
          ++deitr;
        } else {
          deitr = core2_directed[*citr].erase(deitr);  // remove
        }
      }
    }
    global_edges_remain = comm_world().all_reduce(local_edges_remain, MPI_SUM);
  } while (global_edges_remain > 0);
  double total_ktruss_end_time = MPI_Wtime();
  if (comm_world().rank() == 0) {
    std::cout << "TOTAL KTRUSS TIME = "
              << total_ktruss_end_time - total_ktruss_start_time << std::endl;
  }

  // uint64_t global_triangle_count = mpi_all_reduce(
  //     local_triangle_count, std::plus<uint64_t>(), MPI_COMM_WORLD);
  return 0;  // global_triangle_count;
}

}  // end namespace havoqgt