############ TINYXML2 #############

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)


set(TinyXml2Hdr ${CMAKE_CURRENT_LIST_DIR}/include)
set(TinyXml2Src ${CMAKE_CURRENT_LIST_DIR}/source)

set( TinyXml2Srcs
        ${TinyXml2Src}/tinyxml2.cpp
        )

set( TinyXml2Hdrs
        ${TinyXml2Hdr}/tinyxml2.h
        )


source_group("ThirdParty\\TinyXml2\\Include" FILES
    ${TinyXml2Hdrs}
)

source_group("ThirdParty\\TinyXml2\\Source" FILES
    ${TinyXml2Srcs}
    )

set ( TinyXmlIncludes
    ${TinyXml2Hdrs}
    ${TinyXml2Srcs}
    )

############ TINYXML2 END #############