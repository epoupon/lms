// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: messages.proto

#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION
#include "messages.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)

namespace Remote {

namespace {

const ::google::protobuf::Descriptor* ClientMessage_descriptor_ = NULL;
const ::google::protobuf::internal::GeneratedMessageReflection*
  ClientMessage_reflection_ = NULL;
const ::google::protobuf::EnumDescriptor* ClientMessage_Type_descriptor_ = NULL;
const ::google::protobuf::Descriptor* ServerMessage_descriptor_ = NULL;
const ::google::protobuf::internal::GeneratedMessageReflection*
  ServerMessage_reflection_ = NULL;
const ::google::protobuf::EnumDescriptor* ServerMessage_Type_descriptor_ = NULL;

}  // namespace


void protobuf_AssignDesc_messages_2eproto() {
  protobuf_AddDesc_messages_2eproto();
  const ::google::protobuf::FileDescriptor* file =
    ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName(
      "messages.proto");
  GOOGLE_CHECK(file != NULL);
  ClientMessage_descriptor_ = file->message_type(0);
  static const int ClientMessage_offsets_[4] = {
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, type_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, auth_request_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, audio_collection_request_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, media_request_),
  };
  ClientMessage_reflection_ =
    new ::google::protobuf::internal::GeneratedMessageReflection(
      ClientMessage_descriptor_,
      ClientMessage::default_instance_,
      ClientMessage_offsets_,
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, _has_bits_[0]),
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ClientMessage, _unknown_fields_),
      -1,
      ::google::protobuf::DescriptorPool::generated_pool(),
      ::google::protobuf::MessageFactory::generated_factory(),
      sizeof(ClientMessage));
  ClientMessage_Type_descriptor_ = ClientMessage_descriptor_->enum_type(0);
  ServerMessage_descriptor_ = file->message_type(1);
  static const int ServerMessage_offsets_[4] = {
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, type_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, auth_response_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, audio_collection_response_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, media_response_),
  };
  ServerMessage_reflection_ =
    new ::google::protobuf::internal::GeneratedMessageReflection(
      ServerMessage_descriptor_,
      ServerMessage::default_instance_,
      ServerMessage_offsets_,
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, _has_bits_[0]),
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ServerMessage, _unknown_fields_),
      -1,
      ::google::protobuf::DescriptorPool::generated_pool(),
      ::google::protobuf::MessageFactory::generated_factory(),
      sizeof(ServerMessage));
  ServerMessage_Type_descriptor_ = ServerMessage_descriptor_->enum_type(0);
}

namespace {

GOOGLE_PROTOBUF_DECLARE_ONCE(protobuf_AssignDescriptors_once_);
inline void protobuf_AssignDescriptorsOnce() {
  ::google::protobuf::GoogleOnceInit(&protobuf_AssignDescriptors_once_,
                 &protobuf_AssignDesc_messages_2eproto);
}

void protobuf_RegisterTypes(const ::std::string&) {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
    ClientMessage_descriptor_, &ClientMessage::default_instance());
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
    ServerMessage_descriptor_, &ServerMessage::default_instance());
}

}  // namespace

void protobuf_ShutdownFile_messages_2eproto() {
  delete ClientMessage::default_instance_;
  delete ClientMessage_reflection_;
  delete ServerMessage::default_instance_;
  delete ServerMessage_reflection_;
}

void protobuf_AddDesc_messages_2eproto() {
  static bool already_here = false;
  if (already_here) return;
  already_here = true;
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  ::Remote::protobuf_AddDesc_common_2eproto();
  ::Remote::protobuf_AddDesc_auth_2eproto();
  ::Remote::protobuf_AddDesc_collection_2eproto();
  ::Remote::protobuf_AddDesc_media_2eproto();
  ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
    "\n\016messages.proto\022\006Remote\032\014common.proto\032\n"
    "auth.proto\032\020collection.proto\032\013media.prot"
    "o\"\232\002\n\rClientMessage\022(\n\004type\030\001 \002(\0162\032.Remo"
    "te.ClientMessage.Type\022)\n\014auth_request\030\002 "
    "\001(\0132\023.Remote.AuthRequest\022@\n\030audio_collec"
    "tion_request\030\003 \001(\0132\036.Remote.AudioCollect"
    "ionRequest\022+\n\rmedia_request\030\004 \001(\0132\024.Remo"
    "te.MediaRequest\"E\n\004Type\022\017\n\013AuthRequest\020\001"
    "\022\032\n\026AudioCollectionRequest\020\002\022\020\n\014MediaReq"
    "uest\020\003\"\243\002\n\rServerMessage\022(\n\004type\030\001 \002(\0162\032"
    ".Remote.ServerMessage.Type\022+\n\rauth_respo"
    "nse\030\002 \001(\0132\024.Remote.AuthResponse\022B\n\031audio"
    "_collection_response\030\003 \001(\0132\037.Remote.Audi"
    "oCollectionResponse\022-\n\016media_response\030\004 "
    "\001(\0132\025.Remote.MediaResponse\"H\n\004Type\022\020\n\014Au"
    "thResponse\020\001\022\033\n\027AudioCollectionResponse\020"
    "\002\022\021\n\rMediaResponse\020\003", 660);
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(
    "messages.proto", &protobuf_RegisterTypes);
  ClientMessage::default_instance_ = new ClientMessage();
  ServerMessage::default_instance_ = new ServerMessage();
  ClientMessage::default_instance_->InitAsDefaultInstance();
  ServerMessage::default_instance_->InitAsDefaultInstance();
  ::google::protobuf::internal::OnShutdown(&protobuf_ShutdownFile_messages_2eproto);
}

// Force AddDescriptors() to be called at static initialization time.
struct StaticDescriptorInitializer_messages_2eproto {
  StaticDescriptorInitializer_messages_2eproto() {
    protobuf_AddDesc_messages_2eproto();
  }
} static_descriptor_initializer_messages_2eproto_;

// ===================================================================

const ::google::protobuf::EnumDescriptor* ClientMessage_Type_descriptor() {
  protobuf_AssignDescriptorsOnce();
  return ClientMessage_Type_descriptor_;
}
bool ClientMessage_Type_IsValid(int value) {
  switch(value) {
    case 1:
    case 2:
    case 3:
      return true;
    default:
      return false;
  }
}

#ifndef _MSC_VER
const ClientMessage_Type ClientMessage::AuthRequest;
const ClientMessage_Type ClientMessage::AudioCollectionRequest;
const ClientMessage_Type ClientMessage::MediaRequest;
const ClientMessage_Type ClientMessage::Type_MIN;
const ClientMessage_Type ClientMessage::Type_MAX;
const int ClientMessage::Type_ARRAYSIZE;
#endif  // _MSC_VER
#ifndef _MSC_VER
const int ClientMessage::kTypeFieldNumber;
const int ClientMessage::kAuthRequestFieldNumber;
const int ClientMessage::kAudioCollectionRequestFieldNumber;
const int ClientMessage::kMediaRequestFieldNumber;
#endif  // !_MSC_VER

ClientMessage::ClientMessage()
  : ::google::protobuf::Message() {
  SharedCtor();
}

void ClientMessage::InitAsDefaultInstance() {
  auth_request_ = const_cast< ::Remote::AuthRequest*>(&::Remote::AuthRequest::default_instance());
  audio_collection_request_ = const_cast< ::Remote::AudioCollectionRequest*>(&::Remote::AudioCollectionRequest::default_instance());
  media_request_ = const_cast< ::Remote::MediaRequest*>(&::Remote::MediaRequest::default_instance());
}

ClientMessage::ClientMessage(const ClientMessage& from)
  : ::google::protobuf::Message() {
  SharedCtor();
  MergeFrom(from);
}

void ClientMessage::SharedCtor() {
  _cached_size_ = 0;
  type_ = 1;
  auth_request_ = NULL;
  audio_collection_request_ = NULL;
  media_request_ = NULL;
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

ClientMessage::~ClientMessage() {
  SharedDtor();
}

void ClientMessage::SharedDtor() {
  if (this != default_instance_) {
    delete auth_request_;
    delete audio_collection_request_;
    delete media_request_;
  }
}

void ClientMessage::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const ::google::protobuf::Descriptor* ClientMessage::descriptor() {
  protobuf_AssignDescriptorsOnce();
  return ClientMessage_descriptor_;
}

const ClientMessage& ClientMessage::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_messages_2eproto();
  return *default_instance_;
}

ClientMessage* ClientMessage::default_instance_ = NULL;

ClientMessage* ClientMessage::New() const {
  return new ClientMessage;
}

void ClientMessage::Clear() {
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    type_ = 1;
    if (has_auth_request()) {
      if (auth_request_ != NULL) auth_request_->::Remote::AuthRequest::Clear();
    }
    if (has_audio_collection_request()) {
      if (audio_collection_request_ != NULL) audio_collection_request_->::Remote::AudioCollectionRequest::Clear();
    }
    if (has_media_request()) {
      if (media_request_ != NULL) media_request_->::Remote::MediaRequest::Clear();
    }
  }
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
  mutable_unknown_fields()->Clear();
}

bool ClientMessage::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // required .Remote.ClientMessage.Type type = 1;
      case 1: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
          int value;
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   int, ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>(
                 input, &value)));
          if (::Remote::ClientMessage_Type_IsValid(value)) {
            set_type(static_cast< ::Remote::ClientMessage_Type >(value));
          } else {
            mutable_unknown_fields()->AddVarint(1, value);
          }
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(18)) goto parse_auth_request;
        break;
      }

      // optional .Remote.AuthRequest auth_request = 2;
      case 2: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_auth_request:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_auth_request()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_audio_collection_request;
        break;
      }

      // optional .Remote.AudioCollectionRequest audio_collection_request = 3;
      case 3: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_audio_collection_request:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_audio_collection_request()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(34)) goto parse_media_request;
        break;
      }

      // optional .Remote.MediaRequest media_request = 4;
      case 4: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_media_request:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_media_request()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectAtEnd()) return true;
        break;
      }

      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormat::SkipField(
              input, tag, mutable_unknown_fields()));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void ClientMessage::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // required .Remote.ClientMessage.Type type = 1;
  if (has_type()) {
    ::google::protobuf::internal::WireFormatLite::WriteEnum(
      1, this->type(), output);
  }

  // optional .Remote.AuthRequest auth_request = 2;
  if (has_auth_request()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      2, this->auth_request(), output);
  }

  // optional .Remote.AudioCollectionRequest audio_collection_request = 3;
  if (has_audio_collection_request()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      3, this->audio_collection_request(), output);
  }

  // optional .Remote.MediaRequest media_request = 4;
  if (has_media_request()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      4, this->media_request(), output);
  }

  if (!unknown_fields().empty()) {
    ::google::protobuf::internal::WireFormat::SerializeUnknownFields(
        unknown_fields(), output);
  }
}

::google::protobuf::uint8* ClientMessage::SerializeWithCachedSizesToArray(
    ::google::protobuf::uint8* target) const {
  // required .Remote.ClientMessage.Type type = 1;
  if (has_type()) {
    target = ::google::protobuf::internal::WireFormatLite::WriteEnumToArray(
      1, this->type(), target);
  }

  // optional .Remote.AuthRequest auth_request = 2;
  if (has_auth_request()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        2, this->auth_request(), target);
  }

  // optional .Remote.AudioCollectionRequest audio_collection_request = 3;
  if (has_audio_collection_request()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        3, this->audio_collection_request(), target);
  }

  // optional .Remote.MediaRequest media_request = 4;
  if (has_media_request()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        4, this->media_request(), target);
  }

  if (!unknown_fields().empty()) {
    target = ::google::protobuf::internal::WireFormat::SerializeUnknownFieldsToArray(
        unknown_fields(), target);
  }
  return target;
}

int ClientMessage::ByteSize() const {
  int total_size = 0;

  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // required .Remote.ClientMessage.Type type = 1;
    if (has_type()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::EnumSize(this->type());
    }

    // optional .Remote.AuthRequest auth_request = 2;
    if (has_auth_request()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->auth_request());
    }

    // optional .Remote.AudioCollectionRequest audio_collection_request = 3;
    if (has_audio_collection_request()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->audio_collection_request());
    }

    // optional .Remote.MediaRequest media_request = 4;
    if (has_media_request()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->media_request());
    }

  }
  if (!unknown_fields().empty()) {
    total_size +=
      ::google::protobuf::internal::WireFormat::ComputeUnknownFieldsSize(
        unknown_fields());
  }
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void ClientMessage::MergeFrom(const ::google::protobuf::Message& from) {
  GOOGLE_CHECK_NE(&from, this);
  const ClientMessage* source =
    ::google::protobuf::internal::dynamic_cast_if_available<const ClientMessage*>(
      &from);
  if (source == NULL) {
    ::google::protobuf::internal::ReflectionOps::Merge(from, this);
  } else {
    MergeFrom(*source);
  }
}

void ClientMessage::MergeFrom(const ClientMessage& from) {
  GOOGLE_CHECK_NE(&from, this);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_type()) {
      set_type(from.type());
    }
    if (from.has_auth_request()) {
      mutable_auth_request()->::Remote::AuthRequest::MergeFrom(from.auth_request());
    }
    if (from.has_audio_collection_request()) {
      mutable_audio_collection_request()->::Remote::AudioCollectionRequest::MergeFrom(from.audio_collection_request());
    }
    if (from.has_media_request()) {
      mutable_media_request()->::Remote::MediaRequest::MergeFrom(from.media_request());
    }
  }
  mutable_unknown_fields()->MergeFrom(from.unknown_fields());
}

void ClientMessage::CopyFrom(const ::google::protobuf::Message& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

void ClientMessage::CopyFrom(const ClientMessage& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ClientMessage::IsInitialized() const {
  if ((_has_bits_[0] & 0x00000001) != 0x00000001) return false;

  if (has_auth_request()) {
    if (!this->auth_request().IsInitialized()) return false;
  }
  if (has_audio_collection_request()) {
    if (!this->audio_collection_request().IsInitialized()) return false;
  }
  if (has_media_request()) {
    if (!this->media_request().IsInitialized()) return false;
  }
  return true;
}

void ClientMessage::Swap(ClientMessage* other) {
  if (other != this) {
    std::swap(type_, other->type_);
    std::swap(auth_request_, other->auth_request_);
    std::swap(audio_collection_request_, other->audio_collection_request_);
    std::swap(media_request_, other->media_request_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    _unknown_fields_.Swap(&other->_unknown_fields_);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::google::protobuf::Metadata ClientMessage::GetMetadata() const {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::Metadata metadata;
  metadata.descriptor = ClientMessage_descriptor_;
  metadata.reflection = ClientMessage_reflection_;
  return metadata;
}


// ===================================================================

const ::google::protobuf::EnumDescriptor* ServerMessage_Type_descriptor() {
  protobuf_AssignDescriptorsOnce();
  return ServerMessage_Type_descriptor_;
}
bool ServerMessage_Type_IsValid(int value) {
  switch(value) {
    case 1:
    case 2:
    case 3:
      return true;
    default:
      return false;
  }
}

#ifndef _MSC_VER
const ServerMessage_Type ServerMessage::AuthResponse;
const ServerMessage_Type ServerMessage::AudioCollectionResponse;
const ServerMessage_Type ServerMessage::MediaResponse;
const ServerMessage_Type ServerMessage::Type_MIN;
const ServerMessage_Type ServerMessage::Type_MAX;
const int ServerMessage::Type_ARRAYSIZE;
#endif  // _MSC_VER
#ifndef _MSC_VER
const int ServerMessage::kTypeFieldNumber;
const int ServerMessage::kAuthResponseFieldNumber;
const int ServerMessage::kAudioCollectionResponseFieldNumber;
const int ServerMessage::kMediaResponseFieldNumber;
#endif  // !_MSC_VER

ServerMessage::ServerMessage()
  : ::google::protobuf::Message() {
  SharedCtor();
}

void ServerMessage::InitAsDefaultInstance() {
  auth_response_ = const_cast< ::Remote::AuthResponse*>(&::Remote::AuthResponse::default_instance());
  audio_collection_response_ = const_cast< ::Remote::AudioCollectionResponse*>(&::Remote::AudioCollectionResponse::default_instance());
  media_response_ = const_cast< ::Remote::MediaResponse*>(&::Remote::MediaResponse::default_instance());
}

ServerMessage::ServerMessage(const ServerMessage& from)
  : ::google::protobuf::Message() {
  SharedCtor();
  MergeFrom(from);
}

void ServerMessage::SharedCtor() {
  _cached_size_ = 0;
  type_ = 1;
  auth_response_ = NULL;
  audio_collection_response_ = NULL;
  media_response_ = NULL;
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

ServerMessage::~ServerMessage() {
  SharedDtor();
}

void ServerMessage::SharedDtor() {
  if (this != default_instance_) {
    delete auth_response_;
    delete audio_collection_response_;
    delete media_response_;
  }
}

void ServerMessage::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const ::google::protobuf::Descriptor* ServerMessage::descriptor() {
  protobuf_AssignDescriptorsOnce();
  return ServerMessage_descriptor_;
}

const ServerMessage& ServerMessage::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_messages_2eproto();
  return *default_instance_;
}

ServerMessage* ServerMessage::default_instance_ = NULL;

ServerMessage* ServerMessage::New() const {
  return new ServerMessage;
}

void ServerMessage::Clear() {
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    type_ = 1;
    if (has_auth_response()) {
      if (auth_response_ != NULL) auth_response_->::Remote::AuthResponse::Clear();
    }
    if (has_audio_collection_response()) {
      if (audio_collection_response_ != NULL) audio_collection_response_->::Remote::AudioCollectionResponse::Clear();
    }
    if (has_media_response()) {
      if (media_response_ != NULL) media_response_->::Remote::MediaResponse::Clear();
    }
  }
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
  mutable_unknown_fields()->Clear();
}

bool ServerMessage::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // required .Remote.ServerMessage.Type type = 1;
      case 1: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
          int value;
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   int, ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>(
                 input, &value)));
          if (::Remote::ServerMessage_Type_IsValid(value)) {
            set_type(static_cast< ::Remote::ServerMessage_Type >(value));
          } else {
            mutable_unknown_fields()->AddVarint(1, value);
          }
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(18)) goto parse_auth_response;
        break;
      }

      // optional .Remote.AuthResponse auth_response = 2;
      case 2: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_auth_response:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_auth_response()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_audio_collection_response;
        break;
      }

      // optional .Remote.AudioCollectionResponse audio_collection_response = 3;
      case 3: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_audio_collection_response:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_audio_collection_response()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(34)) goto parse_media_response;
        break;
      }

      // optional .Remote.MediaResponse media_response = 4;
      case 4: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_media_response:
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_media_response()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectAtEnd()) return true;
        break;
      }

      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormat::SkipField(
              input, tag, mutable_unknown_fields()));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void ServerMessage::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // required .Remote.ServerMessage.Type type = 1;
  if (has_type()) {
    ::google::protobuf::internal::WireFormatLite::WriteEnum(
      1, this->type(), output);
  }

  // optional .Remote.AuthResponse auth_response = 2;
  if (has_auth_response()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      2, this->auth_response(), output);
  }

  // optional .Remote.AudioCollectionResponse audio_collection_response = 3;
  if (has_audio_collection_response()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      3, this->audio_collection_response(), output);
  }

  // optional .Remote.MediaResponse media_response = 4;
  if (has_media_response()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      4, this->media_response(), output);
  }

  if (!unknown_fields().empty()) {
    ::google::protobuf::internal::WireFormat::SerializeUnknownFields(
        unknown_fields(), output);
  }
}

::google::protobuf::uint8* ServerMessage::SerializeWithCachedSizesToArray(
    ::google::protobuf::uint8* target) const {
  // required .Remote.ServerMessage.Type type = 1;
  if (has_type()) {
    target = ::google::protobuf::internal::WireFormatLite::WriteEnumToArray(
      1, this->type(), target);
  }

  // optional .Remote.AuthResponse auth_response = 2;
  if (has_auth_response()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        2, this->auth_response(), target);
  }

  // optional .Remote.AudioCollectionResponse audio_collection_response = 3;
  if (has_audio_collection_response()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        3, this->audio_collection_response(), target);
  }

  // optional .Remote.MediaResponse media_response = 4;
  if (has_media_response()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        4, this->media_response(), target);
  }

  if (!unknown_fields().empty()) {
    target = ::google::protobuf::internal::WireFormat::SerializeUnknownFieldsToArray(
        unknown_fields(), target);
  }
  return target;
}

int ServerMessage::ByteSize() const {
  int total_size = 0;

  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // required .Remote.ServerMessage.Type type = 1;
    if (has_type()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::EnumSize(this->type());
    }

    // optional .Remote.AuthResponse auth_response = 2;
    if (has_auth_response()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->auth_response());
    }

    // optional .Remote.AudioCollectionResponse audio_collection_response = 3;
    if (has_audio_collection_response()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->audio_collection_response());
    }

    // optional .Remote.MediaResponse media_response = 4;
    if (has_media_response()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->media_response());
    }

  }
  if (!unknown_fields().empty()) {
    total_size +=
      ::google::protobuf::internal::WireFormat::ComputeUnknownFieldsSize(
        unknown_fields());
  }
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void ServerMessage::MergeFrom(const ::google::protobuf::Message& from) {
  GOOGLE_CHECK_NE(&from, this);
  const ServerMessage* source =
    ::google::protobuf::internal::dynamic_cast_if_available<const ServerMessage*>(
      &from);
  if (source == NULL) {
    ::google::protobuf::internal::ReflectionOps::Merge(from, this);
  } else {
    MergeFrom(*source);
  }
}

void ServerMessage::MergeFrom(const ServerMessage& from) {
  GOOGLE_CHECK_NE(&from, this);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_type()) {
      set_type(from.type());
    }
    if (from.has_auth_response()) {
      mutable_auth_response()->::Remote::AuthResponse::MergeFrom(from.auth_response());
    }
    if (from.has_audio_collection_response()) {
      mutable_audio_collection_response()->::Remote::AudioCollectionResponse::MergeFrom(from.audio_collection_response());
    }
    if (from.has_media_response()) {
      mutable_media_response()->::Remote::MediaResponse::MergeFrom(from.media_response());
    }
  }
  mutable_unknown_fields()->MergeFrom(from.unknown_fields());
}

void ServerMessage::CopyFrom(const ::google::protobuf::Message& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

void ServerMessage::CopyFrom(const ServerMessage& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ServerMessage::IsInitialized() const {
  if ((_has_bits_[0] & 0x00000001) != 0x00000001) return false;

  if (has_auth_response()) {
    if (!this->auth_response().IsInitialized()) return false;
  }
  if (has_audio_collection_response()) {
    if (!this->audio_collection_response().IsInitialized()) return false;
  }
  if (has_media_response()) {
    if (!this->media_response().IsInitialized()) return false;
  }
  return true;
}

void ServerMessage::Swap(ServerMessage* other) {
  if (other != this) {
    std::swap(type_, other->type_);
    std::swap(auth_response_, other->auth_response_);
    std::swap(audio_collection_response_, other->audio_collection_response_);
    std::swap(media_response_, other->media_response_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    _unknown_fields_.Swap(&other->_unknown_fields_);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::google::protobuf::Metadata ServerMessage::GetMetadata() const {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::Metadata metadata;
  metadata.descriptor = ServerMessage_descriptor_;
  metadata.reflection = ServerMessage_reflection_;
  return metadata;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace Remote

// @@protoc_insertion_point(global_scope)
