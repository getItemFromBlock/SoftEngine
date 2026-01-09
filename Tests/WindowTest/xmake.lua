
target("WindowTest")
	set_kind("binary")

	add_deps("Engine")
	add_includedirs("../../Engine/src")

	add_files("test_window.cpp")

	add_packages("galaxymath")
	add_packages("thread-pool")
	add_packages("gtest")
target_end()