{
	"targets": [
		{
			"target_name": "nodeca",
			"include_dirs": ["/afs/slac/g/lcls/epics/base/base-R3-14-8-2-lcls6/include", "/afs/slac/g/lcls/epics/base/base-R3-14-8-2-lcls6/include/os/Linux"],
			"cflags": ['-g'],
			"sources": [ "nodeca.cc", "pvobject.cc" ],
			"link_settings": {
				"libraries": ["-L/afs/slac/g/lcls/epics/base/base-R3-14-8-2-lcls6/lib/linux-x86","-lca","-lCom"]
			}

		}
	]
}
