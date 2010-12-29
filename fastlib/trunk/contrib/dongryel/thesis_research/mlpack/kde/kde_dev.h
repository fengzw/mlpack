/** @file kde_dev.h
 *
 *  The kernel density estimator object that processes user inputs and
 *  to produce the computation results.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef MLPACK_KDE_KDE_DEV_H
#define MLPACK_KDE_KDE_DEV_H

#include "core/gnp/dualtree_dfs_dev.h"
#include "core/metric_kernels/lmetric.h"
#include "mlpack/kde/kde.h"

namespace mlpack {
namespace kde {
template<typename TableType>
TableType *Kde<TableType>::query_table() {
  return query_table_;
}

template<typename TableType>
TableType *Kde<TableType>::reference_table() {
  return reference_table_;
}

template<typename TableType>
typename Kde<TableType>::GlobalType &Kde<TableType>::global() {
  return global_;
}

template<typename TableType>
bool Kde<TableType>::is_monochromatic() const {
  return is_monochromatic_;
}

template<typename TableType>
void Kde<TableType>::Compute(
  const KdeArguments<TableType> &arguments_in,
  KdeResult< std::vector<double> > *result_out) {

  // Instantiate a dual-tree algorithm of the KDE.
  typedef Kde<TableType> ProblemType;
  core::gnp::DualtreeDfs< ProblemType > dualtree_dfs;
  dualtree_dfs.Init(*this);

  // Compute the result.
  if(arguments_in.num_iterations_in_ <= 0) {
    dualtree_dfs.Compute(* arguments_in.metric_, result_out);
    printf("Number of prunes: %d\n", dualtree_dfs.num_deterministic_prunes());
  }
  else {
    typename core::gnp::DualtreeDfs<ProblemType>::iterator kde_it =
      dualtree_dfs.get_iterator(*arguments_in.metric_, result_out);
    for(int i = 0; i < arguments_in.num_iterations_in_; i++) {
      ++kde_it;
    }

    // Tell the iterator that we are done using it so that the
    // result can be finalized.
    kde_it.Finalize();
  }
}

template<typename TableType>
void Kde<TableType>::Init(
  KdeArguments<TableType> &arguments_in) {

  reference_table_ = arguments_in.reference_table_;
  if(arguments_in.query_table_ == arguments_in.reference_table_) {
    is_monochromatic_ = true;
    query_table_ = reference_table_;
  }
  else {
    is_monochromatic_ = false;
    query_table_ = arguments_in.query_table_;
  }

  // Declare the global constants.
  global_.Init(
    reference_table_, query_table_,
    arguments_in.effective_num_reference_points_,
    arguments_in.bandwidth_, is_monochromatic_,
    arguments_in.relative_error_, arguments_in.probability_,
    arguments_in.kernel_, arguments_in.normalize_densities_);
}

template<typename TableType>
void Kde<TableType>::set_bandwidth(double bandwidth_in) {
  global_.set_bandwidth(bandwidth_in);
}

template<typename TableType>
bool Kde<TableType>::ConstructBoostVariableMap_(
  const std::vector<std::string> &args,
  boost::program_options::variables_map *vm) {

  boost::program_options::options_description desc("Available options");
  desc.add_options()(
    "help", "Print this information."
  )(
    "references_in",
    boost::program_options::value<std::string>(),
    "REQUIRED file containing reference data."
  )(
    "queries_in",
    boost::program_options::value<std::string>(),
    "OPTIONAL file containing query positions.  If omitted, KDE computes "
    "the leave-one-out density at each reference point."
  )(
    "densities_out",
    boost::program_options::value<std::string>()->default_value(
      "densities_out.csv"),
    "OPTIONAL file to store computed densities."
  )(
    "kernel",
    boost::program_options::value<std::string>()->default_value("epan"),
    "Kernel function used by KDE.  One of:\n"
    "  epan, gaussian"
  )(
    "bandwidth",
    boost::program_options::value<double>(),
    "OPTIONAL kernel bandwidth, if you set --bandwidth_selection flag, "
    "then the --bandwidth will be ignored."
  )(
    "num_iterations_in",
    boost::program_options::value<int>()->default_value(0),
    "The number of iterations to run."
  )(
    "probability",
    boost::program_options::value<double>()->default_value(1.0),
    "Probability guarantee for the approximation of KDE."
  )(
    "relative_error",
    boost::program_options::value<double>()->default_value(0.01),
    "Relative error for the approximation of KDE."
  )(
    "leaf_size",
    boost::program_options::value<int>()->default_value(20),
    "Maximum number of points at a leaf of the tree."
  );

  boost::program_options::command_line_parser clp(args);
  clp.style(boost::program_options::command_line_style::default_style
            ^ boost::program_options::command_line_style::allow_guessing);
  try {
    boost::program_options::store(clp.options(desc).run(), *vm);
  }
  catch(const boost::program_options::invalid_option_value &e) {
    std::cerr << "Invalid Argument: " << e.what() << "\n";
    exit(0);
  }
  catch(const boost::program_options::invalid_command_line_syntax &e) {
    std::cerr << "Invalid command line syntax: " << e.what() << "\n";
    exit(0);
  }
  catch(const boost::program_options::unknown_option &e) {
    std::cerr << "Unknown option: " << e.what() << "\n";
    exit(0);
  }

  boost::program_options::notify(*vm);
  if(vm->count("help")) {
    std::cout << desc << "\n";
    return true;
  }

  // Validate the arguments. Only immediate dying is allowed here, the
  // parsing is done later.
  if(vm->count("references_in") == 0) {
    std::cerr << "Missing required --references_in.\n";
    exit(0);
  }
  if((*vm)["kernel"].as<std::string>() != "gaussian" &&
      (*vm)["kernel"].as<std::string>() != "epan") {
    std::cerr << "We support only epan or gaussian for the kernel.\n";
    exit(0);
  }
  if(vm->count("bandwidth") > 0 && (*vm)["bandwidth"].as<double>() <= 0) {
    std::cerr << "The --bandwidth requires a positive real number.\n";
    exit(0);
  }
  if(vm->count("bandwidth") == 0) {
    std::cerr << "Missing required --bandwidth.\n";
    exit(0);
  }
  if((*vm)["probability"].as<double>() <= 0 ||
      (*vm)["probability"].as<double>() > 1) {
    std::cerr << "The --probability requires a real number $0 < p <= 1$.\n";
    exit(0);
  }
  if((*vm)["relative_error"].as<double>() < 0) {
    std::cerr << "The --relative_error requires a real number $r >= 0$.\n";
    exit(0);
  }
  if((*vm)["leaf_size"].as<int>() <= 0) {
    std::cerr << "The --leaf_size needs to be a positive integer.\n";
    exit(0);
  }
  return false;
}

template<typename TableType>
bool Kde<TableType>::ParseArguments(
  const std::vector<std::string> &args,
  KdeArguments<TableType> *arguments_out) {

  // A L2 metric to index the table to use.
  arguments_out->metric_ = new core::metric_kernels::LMetric<2>();

  // Construct the Boost variable map.
  boost::program_options::variables_map vm;
  if(ConstructBoostVariableMap_(args, &vm)) {
    return true;
  }

  // Given the constructed boost variable map, parse each argument.

  // Parse the densities out file.
  arguments_out->densities_out_ = vm["densities_out"].as<std::string>();

  // Parse the leaf size.
  arguments_out->leaf_size_ = vm["leaf_size"].as<int>();
  std::cout << "Using the leaf size of " << arguments_out->leaf_size_ << "\n";

  // Parse the reference set and index the tree.
  std::cout << "Reading in the reference set: " <<
            vm["references_in"].as<std::string>() << "\n";
  arguments_out->reference_table_ = new TableType();
  arguments_out->reference_table_->Init(vm["references_in"].as<std::string>());
  std::cout << "Finished reading in the reference set.\n";
  std::cout << "Building the reference tree.\n";
  arguments_out->reference_table_->IndexData(
    *(arguments_out->metric_), arguments_out->leaf_size_);
  std::cout << "Finished building the reference tree.\n";

  // Parse the query set and index the tree.
  if(vm.count("queries_in") > 0) {
    std::cout << "Reading in the query set: " <<
              vm["queries_in"].as<std::string>() << "\n";
    arguments_out->query_table_ =
      arguments_out->query_table_ = new TableType();
    arguments_out->query_table_->Init(vm["queries_in"].as<std::string>());
    std::cout << "Finished reading in the query set.\n";
    std::cout << "Building the query tree.\n";
    arguments_out->query_table_->IndexData(
      *(arguments_out->metric_), arguments_out->leaf_size_);
    std::cout << "Finished building the query tree.\n";
    arguments_out->effective_num_reference_points_ =
      arguments_out->reference_table_->n_entries();
  }
  else {
    arguments_out->query_table_ = arguments_out->reference_table_;
    arguments_out->effective_num_reference_points_ =
      arguments_out->reference_table_->n_entries() - 1;
  }

  // Parse the bandwidth.
  arguments_out->bandwidth_ = vm["bandwidth"].as<double>();
  std::cout << "Bandwidth of " << arguments_out->bandwidth_ << "\n";

  // Parse the relative error.
  arguments_out->relative_error_ = vm["relative_error"].as<double>();
  std::cout << "Relative error of " << arguments_out->relative_error_ << "\n";

  // Parse the probability.
  arguments_out->probability_ = vm["probability"].as<double>();
  std::cout << "Probability of " << arguments_out->probability_ << "\n";

  // Parse the kernel type.
  arguments_out->kernel_ = vm["kernel"].as< std::string >();
  std::cout << "Using the kernel: " << arguments_out->kernel_ << "\n";

  // Parse the number of iterations.
  arguments_out->num_iterations_in_ = vm["num_iterations_in"].as<int>();
  if(arguments_out->num_iterations_in_ > 0) {
    std::cout << "Running for " << arguments_out->num_iterations_in_ <<
              " iterations on a progressive mode...\n";
  }
  else {
    std::cout << "Running the algorithm on a non-progressive mode...\n";
  }
  return false;
}

template<typename TableType>
bool Kde<TableType>::ParseArguments(
  int argc,
  char *argv[],
  KdeArguments<TableType> *arguments_out) {

  // Convert C input to C++; skip executable name for Boost.
  std::vector<std::string> args(argv + 1, argv + argc);

  return ParseArguments(args, arguments_out);
}
};
};

#endif
