/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <rpearce@llnl.gov>.
 * LLNL-CODE-644630.
 * All rights reserved.
 *
 * This file is part of HavoqGT, Version 0.1.
 * For details, see https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Our Notice and GNU Lesser General Public License.
 *   http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
 *
 * Our Preamble Notice
 *
 * A. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at the Lawrence
 * Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.
 *
 * B. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately-owned rights.
 *
 * C. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring by
 * the United States Government or Lawrence Livermore National Security, LLC. The
 * views and opinions of authors expressed herein do not necessarily state or
 * reflect those of the United States Government or Lawrence Livermore National
 * Security, LLC, and shall not be used for advertising or product endorsement
 * purposes.
 *
 */

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <havoqgt/delegate_partitioned_graph.hpp>
#include <havoqgt/rmat_edge_generator.hpp>
#include <havoqgt/upper_triangle_edge_generator.hpp>
#include <havoqgt/gen_preferential_attachment_edge_list.hpp>
#include <havoqgt/environment.hpp>
#include <havoqgt/cache_utilities.hpp>
#include <havoqgt/distributed_db.hpp>
#include <iostream>
#include <assert.h>
#include <deque>
#include <utility>
#include <algorithm>
#include <functional>
#include <fstream>      // std::ifstream


// notes for how to setup a good test
// take rank * 100 and make edges between (all local)
// Make one vert per rank a hub.

using namespace havoqgt;
namespace hmpi = havoqgt::mpi;
using namespace havoqgt::mpi;

void usage()  {
  if(havoqgt_env()->world_comm().rank() == 0) {
    std::cerr << "Usage: -s <int> -d <int> -o <string>\n"
         << " -s <int>    - RMAT graph Scale (default 17)\n"
         << " -d <int>    - delegate threshold (Default is 1048576)\n"
         << " -o <string> - output graph base filename\n"
         << " -h          - print help and exit\n\n";
         
  }
}

void parse_cmd_line(int argc, char** argv, uint64_t& scale, uint64_t& delegate_threshold, std::string& output_filename) {
  if(havoqgt_env()->world_comm().rank() == 0) {
    std::cout << "CMD line:";
    for (int i=0; i<argc; ++i) {
      std::cout << " " << argv[i];
    }
    std::cout << std::endl;
  }
  
  bool found_output_filename = false;
  scale = 17;
  delegate_threshold = 1048576;
  
  char c;
  bool prn_help = false;
  while ((c = getopt(argc, argv, "s:d:o:h ")) != -1) {
     switch (c) {
       case 'h':  
         prn_help = true;
         break;
      case 's':
         scale = atoll(optarg);
         break;
      case 'd':
         delegate_threshold = atoll(optarg);
         break; 
      case 'o':
         found_output_filename = true;
         output_filename = optarg;
         break;
      default:
         std::cerr << "Unrecognized option: "<<c<<", ignore."<<std::endl;
         prn_help = true;
         break;
     }
   } 
   if (prn_help || !found_output_filename) {
     usage();
     exit(-1);
   }
}

int main(int argc, char** argv) {

  typedef havoqgt::distributed_db::segment_manager_type segment_manager_t;

  typedef hmpi::delegate_partitioned_graph<segment_manager_t> graph_type;

  int mpi_rank(0), mpi_size(0);

  havoqgt_init(&argc, &argv);
  {
    int mpi_rank = havoqgt_env()->world_comm().rank();
    int mpi_size = havoqgt_env()->world_comm().size();
    havoqgt::get_environment();
    
    if (mpi_rank == 0) {

      std::cout << "MPI initialized with " << mpi_size << " ranks." << std::endl;
      havoqgt::get_environment().print();
    }
    havoqgt_env()->world_comm().barrier();

    uint64_t num_vertices = 1;
    uint64_t vert_scale;
    uint64_t hub_threshold;
    std::string fname_output;
        
    parse_cmd_line(argc, argv, vert_scale, hub_threshold, fname_output);

    num_vertices <<= vert_scale;
    if (mpi_rank == 0) {
      std::cout << "Building Graph500"<< std::endl
        << "Building graph Scale: " << vert_scale << std::endl
        << "Hub threshold = " << hub_threshold << std::endl
        << "File name = " << fname_output << std::endl;
    }

    havoqgt::distributed_db ddb(havoqgt::db_create(), fname_output.c_str());

    segment_manager_t* segment_manager = ddb.get_segment_manager();
    bip::allocator<void, segment_manager_t> alloc_inst(segment_manager);

    //Generate RMAT graph
    uint64_t num_edges_per_rank = num_vertices * 16 / mpi_size;
    havoqgt::rmat_edge_generator rmat(uint64_t(5489) + uint64_t(mpi_rank) * 3ULL,
                                      vert_scale, num_edges_per_rank,
                                      0.57, 0.19, 0.19, 0.05, true, true);


    if (mpi_rank == 0) {
      std::cout << "Generating new graph." << std::endl;
    }
    graph_type *graph = segment_manager->construct<graph_type>
        ("graph_obj")
        (alloc_inst, MPI_COMM_WORLD, rmat, rmat.max_vertex_id(), hub_threshold);


    havoqgt_env()->world_comm().barrier();
    if (mpi_rank == 0) {
      std::cout << "Graph Ready, Calculating Stats. " << std::endl;
    }



    for (int i = 0; i < mpi_size; i++) {
      if (i == mpi_rank) {
        double percent = double(segment_manager->get_free_memory()) /
        double(segment_manager->get_size());
        std::cout << "[" << mpi_rank << "] " << segment_manager->get_free_memory()
                  << "/" << segment_manager->get_size() << " = " << percent << std::endl;
      }
      havoqgt_env()->world_comm().barrier();
    }

    graph->print_graph_statistics();


    //
    // Calculate max degree
    uint64_t max_degree(0);
    for (auto citr = graph->controller_begin(); citr != graph->controller_end(); ++citr) {
      max_degree = std::max(max_degree, graph->degree(*citr));
    }

    uint64_t global_max_degree = havoqgt::mpi::mpi_all_reduce(max_degree, std::greater<uint64_t>(), MPI_COMM_WORLD);

    havoqgt_env()->world_comm().barrier();

    if (mpi_rank == 0) {
      std::cout << "Max Degree = " << global_max_degree << std::endl;
    }

    havoqgt_env()->world_comm().barrier();

  } //END Main MPI
  havoqgt_finalize();
  return 0;
}
