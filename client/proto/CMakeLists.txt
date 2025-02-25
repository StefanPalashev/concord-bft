find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

include_directories(${GRPC_INCLUDE_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CMAKE_CURRENT_BINARY_DIR}
  request/v1/request.proto
  event/v1/event.proto
  state_snapshot/v1/state_snapshot.proto
  health/v1/health.proto
  ../concordclient/proto/concord_client_request/v1/concord_client_request.proto
)
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${CMAKE_CURRENT_BINARY_DIR}
  request/v1/request.proto
  event/v1/event.proto
  state_snapshot/v1/state_snapshot.proto
  health/v1/health.proto
  ../concordclient/proto/concord_client_request/v1/concord_client_request.proto
)

add_library(clientservice-proto STATIC ${PROTO_SRCS} ${GRPC_SRCS})
target_link_libraries(clientservice-proto PRIVATE protobuf::libprotobuf gRPC::grpc++)
target_include_directories(clientservice-proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
