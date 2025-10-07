subdir('libs')

xwebvnc_srcs = assets_sources + [
    'main.c',
    'vncQueue.c',
    'webSocket.c',
    'vncInput.c',
]

# Include your own folder + DIX headers
xwebvnc_inc = include_directories(
    '.'
)

openssl_dep = dependency('openssl', required: true)
jpeg_dep = dependency('libjpeg', required: true)
lz4_dep = dependency('liblz4', required: true)


# Link libxserver_dix using link_with
libxserver_xwebvnc = static_library(
    'libxserver_xwebvnc',
    xwebvnc_srcs,
    
    include_directories: [xwebvnc_inc, inc],
    link_with: [libxserver_dix],
    dependencies: [common_dep, openssl_dep, jpeg_dep, lz4_dep],
    install: false
)
