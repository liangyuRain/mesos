// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __COMMON_COMMAND_UTILS_HPP__
#define __COMMON_COMMAND_UTILS_HPP__

#include <vector>

#include <process/process.hpp>

#include <stout/option.hpp>
#include <stout/path.hpp>

namespace mesos {
namespace internal {
namespace command {

enum class Compression
{
  GZIP,
  BZIP2,
  XZ
};


/**
 * Tar(archive) the file/directory to produce output file.
 *
 * @param input file or directory that will be archived.
 * @param output output archive name.
 * @param directory change to this directory before archiving.
 * @param compression compression type if the archive has to be compressed.
 */
process::Future<Nothing> tar(
    const Path& input,
    const Path& output,
    const Option<Path>& directory = None(),
    const Option<Compression>& compression = None());


/**
 * Untar(unarchive) the given file.
 *
 * @param input file or directory that will be unarchived.
 * @param directory change to this directory before unarchiving.
 */
process::Future<Nothing> untar(
    const Path& input,
    const Option<Path>& directory = None());

// TODO(Jojy): Add more overloads/options for untar (eg., keep existing files)


#ifdef __WINDOWS__
/* Using wclayer from https://github.com/microsoft/hcsshim */

/**
 * creates a new writable container layer
 * 
 * @param directory layer path
 * @param layers paths to the read-only parent layers, the order of the layers
 * matters with base layer should be the last one
 */ 
process::Future<Nothing> wclayer_create(
    const Path& directory,
    const std::vector<Path>& layers);


/**
 * exports a layer to a tar file
 * 
 * @param directory layer path
 * @param layers paths to the read-only parent layers, the order of the layers
 * matters with base layer should be the last one
 * @param output output layer tar
 * @param compress output with gzip compression
 */
process::Future<Nothing> wclayer_export(
    const Path& directory,
    const std::vector<Path>& layers,
    const Path& output,
    bool gzip = false);


/**
 * imports a layer from a tar file
 *
 * @param directory path
 * @param input input layer tar
 * @param layers paths to the read-only parent layers, the order of the layers
 * matters with base layer should be the last one
 */
process::Future<Nothing> wclayer_import(
    const Path& directory,
    const Path& input,
    const std::vector<Path>& layers);


/**
 * mounts a scratch
 * 
 * @param scratch scratch path
 * @param layers paths to the read-only parent layers, the order of the layers
 * matters with base layer should be the last one
 */
process::Future<Nothing> wclayer_mount(
    const Path& scratch,
    const std::vector<Path>& layers);


/**
 * permanently removes a layer directory in its entirety
 *
 * @param directory layer path
 */
process::Future<Nothing> wclayer_remove(const Path& directory);


/**
 * unmounts a scratch
 * 
 * @param directory layer path
 */
process::Future<Nothing> wclayer_unmount(const Path& directory);
#endif // __WINDOWS__


/**
 * Computes SHA 512 checksum of a file.
 *
 * @param input path of the file whose SHA 512 checksum has to be computed.
 */
process::Future<std::string> sha512(const Path& input);


/**
 * Compresses the given input file in GZIP format.
 *
 * @param input path of the file to be compressed.
 */
process::Future<Nothing> gzip(const Path& input);

// TODO(jojy): Add support for other compression algorithms.

/**
 * Decompresses given input file based on its compression format.
 * TODO(jojy): Add support for other compression algorithms.
 *
 * @param input path of the compressed file.
 */
process::Future<Nothing> decompress(const Path& input);

} // namespace command {
} // namespace internal {
} // namespace mesos {

#endif // __COMMON_COMMAND_UTILS_HPP__