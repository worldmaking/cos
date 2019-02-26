{
    "targets": [{
		"target_name": "an",
		"sources": [ 
			"node_module.cpp"
		],
		"include_dirs": [
			".",
			"<!(node -e \"require('nan')\")"
		],
		'cflags': [ '-fexceptions', '-O3' ],
		'cflags_cc': [ '-fexceptions', '-O3' ],
		'conditions': [
			['OS=="mac"', {
				'defines': [],
				'link_settings': {
					'libraries': [
						'-framework Cocoa',
						'-framework IOKit',
						'-framework CoreFoundation',
						'-framework CoreVideo',
						'-L../glfw3/osx', '-lglfw3',
						'-L../libuv/osx', '-luv'
					]
				},
				'xcode_settings': {
					"MACOSX_DEPLOYMENT_TARGET": '10.11',
					"CLANG_CXX_LIBRARY": "libc++",
					'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
				}
			}],
			['OS=="win"', {
				
			}]
		]
	}]
}