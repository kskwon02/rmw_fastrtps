// Copyright 2016-2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "fastdds/rtps/common/SerializedPayload.h"

#include "fastcdr/FastBuffer.h"
#include "fastcdr/Cdr.h"

#include "fastrtps/rtps/common/SerializedPayload.h"
#include "fastrtps/utils/md5.h"
#include "fastrtps/types/TypesBase.h"
#include "fastrtps/types/TypeObjectFactory.h"
#include "fastrtps/types/TypeNamesGenerator.h"
#include "fastrtps/types/AnnotationParameterValue.h"

#include "rmw_fastrtps_shared_cpp/TypeSupport.hpp"
#include "rmw/error_handling.h"

#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"

namespace rmw_fastrtps_shared_cpp
{

TypeSupport::TypeSupport()
{
  m_isGetKeyDefined = false;
  max_size_bound_ = false;
  is_plain_ = false;
  auto_fill_type_object(false);
  auto_fill_type_information(false);
}

void TypeSupport::deleteData(void * data)
{
  assert(data);
  delete static_cast<eprosima::fastcdr::FastBuffer *>(data);
}

void * TypeSupport::createData()
{
  return new eprosima::fastcdr::FastBuffer();
}

bool TypeSupport::serialize(
  void * data, eprosima::fastrtps::rtps::SerializedPayload_t * payload)
{
  assert(data);
  assert(payload);

  auto ser_data = static_cast<SerializedData *>(data);
  if (ser_data->is_cdr_buffer) {
    auto ser = static_cast<eprosima::fastcdr::Cdr *>(ser_data->data);
    if (payload->max_size >= ser->getSerializedDataLength()) {
      payload->length = static_cast<uint32_t>(ser->getSerializedDataLength());
      payload->encapsulation = ser->endianness() ==
        eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
      memcpy(payload->data, ser->getBufferPointer(), ser->getSerializedDataLength());
      return true;
    }
  } else {
    eprosima::fastcdr::FastBuffer fastbuffer(
      reinterpret_cast<char *>(payload->data),
      payload->max_size);  // Object that manages the raw buffer.
    eprosima::fastcdr::Cdr ser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
      eprosima::fastcdr::Cdr::DDS_CDR);  // Object that serializes the data.
    if (this->serializeROSmessage(ser_data->data, ser, ser_data->impl)) {
      payload->encapsulation = ser.endianness() ==
        eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
      payload->length = (uint32_t)ser.getSerializedDataLength();
      return true;
    }
  }

  return false;
}

bool TypeSupport::deserialize(
  eprosima::fastrtps::rtps::SerializedPayload_t * payload,
  void * data)
{
  assert(data);
  assert(payload);

  auto ser_data = static_cast<SerializedData *>(data);
  if (ser_data->is_cdr_buffer) {
    auto buffer = static_cast<eprosima::fastcdr::FastBuffer *>(ser_data->data);
    if (!buffer->reserve(payload->length)) {
      return false;
    }
    memcpy(buffer->getBuffer(), payload->data, payload->length);
    return true;
  }

  eprosima::fastcdr::FastBuffer fastbuffer(
    reinterpret_cast<char *>(payload->data),
    payload->length);
  eprosima::fastcdr::Cdr deser(
    fastbuffer,
    eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
    eprosima::fastcdr::Cdr::DDS_CDR);
  return deserializeROSmessage(deser, ser_data->data, ser_data->impl);
}

std::function<uint32_t()> TypeSupport::getSerializedSizeProvider(void * data)
{
  assert(data);

  auto ser_data = static_cast<SerializedData *>(data);
  auto ser_size = [this, ser_data]() -> uint32_t
    {
      if (ser_data->is_cdr_buffer) {
        auto ser = static_cast<eprosima::fastcdr::Cdr *>(ser_data->data);
        return static_cast<uint32_t>(ser->getSerializedDataLength());
      }
      return static_cast<uint32_t>(
        this->getEstimatedSerializedSize(
          ser_data->data,
          ser_data->impl));
    };
  return ser_size;
}
}  // namespace rmw_fastrtps_shared_cpp