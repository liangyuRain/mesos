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

#include <process/dispatch.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

#include <stout/adaptor.hpp>
#include <stout/foreach.hpp>
#include <stout/fs.hpp>
#include <stout/os.hpp>

#include <stout/os/realpath.hpp>

#include "linux/fs.hpp"

#include "slave/containerizer/mesos/provisioner/backends/wclayer.hpp"

using process::Failure;
using process::Future;
using process::Owned;
using process::Process;
using process::Shared;

using process::dispatch;
using process::spawn;
using process::wait;

using std::string;
using std::vector;

namespace mesos {
namespace internal {
namespace slave {

class WclayerBackendProcess : public Process<WclayerBackendProcess>
{
public:
  WclayerBackendProcess()
    : ProcessBase(process::ID::generate("wclayer-provisioner-backend")) {}

  Future<Nothing> provision(
      const vector<string>& layers,
      const string& rootfs,
      const string& backendDir);

  Future<bool> destroy(
      const string& rootfs,
      const string& backendDir);
};


Try<Owned<Backend>> WclayerBackend::create(const Flags&)
{
  return Owned<Backend>(new WclayerBackend(
      Owned<WclayerBackendProcess>(new WclayerBackendProcess())));
}


WclayerBackend::~WclayerBackend()
{
  terminate(process.get());
  wait(process.get());
}


WclayerBackend::WclayerBackend(Owned<WclayerBackendProcess> _process)
  : process(_process)
{
  spawn(CHECK_NOTNULL(process.get()));
}


Future<Nothing> WclayerBackend::provision(
    const vector<string>& layers,
    const string& rootfs,
    const string& backendDir)
{
  return dispatch(
      process.get(),
      &WclayerBackendProcess::provision,
      layers,
      rootfs,
      backendDir);
}


Future<bool> WclayerBackend::destroy(
    const string& rootfs,
    const string& backendDir)
{
  return dispatch(
      process.get(),
      &WclayerBackendProcess::destroy,
      rootfs,
      backendDir);
}


Future<Nothing> WclayerBackendProcess::provision(
    const vector<string>& layers,
    const string& rootfs,
    const string& backendDir)
{
  if (layers.size() == 0) {
    return Failure("No filesystem layer provided");
  }

  Try<Nothing> mkdir = os::mkdir(rootfs);
  if (mkdir.isError()) {
    return Failure(
        "Failed to create container rootfs at '" +
        rootfs + "': " + mkdir.error());
  }

  const string rootfsId = Path(rootfs).basename();
  const string scratchDir = path::join(backendDir, "scratch", rootfsId);

  // We need the rightmost layer to be the base layer in the argument to
  // `wclayer_create` and `wclayer_mount`, so we need to reverse the order.
  vector<Path> rlayers(layers.rbegin(), layers.rend());

  return command::wclayer_create(scratchDir, rlayers)
    .then(defer(self(), &command::wclayer_mount(scratchDir, rlayers)));
}


Future<bool> WclayerBackendProcess::destroy(
    const string& rootfs,
    const string& backendDir)
{
  const string rootfsId = Path(rootfs).basename();
  const string scratchDir = path::join(backendDir, "scratch", rootfsId);
  
  // TODO: Did not check whether ths directory is mounted or not.
  Future<Nothing> unmount = command::wclayer_unmount(scratchDir);
  unmount.await();
  if (unmount.isFailed()) {
    VLOG(1) << "Failed to unmount scratch directory '" << scratchDir << "': "
            << unmount.Failure();
  }

  
}

} // namespace slave {
} // namespace internal {
} // namespace mesos {
