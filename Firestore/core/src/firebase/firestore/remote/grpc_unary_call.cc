/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Firestore/core/src/firebase/firestore/remote/grpc_unary_call.h"

#include <utility>

#include "Firestore/core/src/firebase/firestore/remote/convert_status.h"

namespace firebase {
namespace firestore {
namespace remote {

using util::AsyncQueue;

GrpcUnaryCall::GrpcUnaryCall(
    std::unique_ptr<grpc::ClientContext> context,
    std::unique_ptr<grpc::GenericClientAsyncResponseReader> call,
    AsyncQueue* worker_queue,
    const grpc::ByteBuffer& message) 
    : context_{std::move(context)},
      call_{std::move(call)},
      worker_queue_{worker_queue},
      message_{message} {
}

GrpcUnaryCall::~GrpcUnaryCall() {
  HARD_ASSERT(!finish_completion_,
              "GrpcUnaryCall is being destroyed without proper shutdown");
}

void GrpcUnaryCall::Start(CallbackT callback) {
  callback_ = callback;
  call_->StartCall();

  finish_completion_ = new GrpcCompletion(
      worker_queue_, [this](bool, const GrpcCompletion* completion) {
        // Ignoring ok; presumably, status is a strict superset.
        finish_completion_ = nullptr;
        callback_(*completion->message(), ConvertStatus(*completion->status()));
        // This `GrpcUnaryCall`'s lifetime might have been ended by the
        // callback.
      });

  call_->Finish(finish_completion_->message(), finish_completion_->status(),
                finish_completion_);
}

void GrpcUnaryCall::Cancel() {
  if (!finish_completion_) {
    // Nothing to cancel.
    return;
  }

  context_->TryCancel();
  finish_completion_->Cancel();
  // This is blocking.
  finish_completion_->WaitUntilOffQueue();
  finish_completion_ = nullptr;
}

GrpcUnaryCall::MetadataT GrpcUnaryCall::GetResponseHeaders() const {
  return context_->GetServerInitialMetadata();
}

}  // namespace remote
}  // namespace firestore
}  // namespace firebase
