extends SceneTree
func _init():
	var pck = OS.get_environment("CATAPULT_TEST_PCK")
	print("PCK_LOAD=", ProjectSettings.load_resource_pack(pck, false))
	for resource_path in ["res://utils/7za", "res://utils/7-ZIP_LICENSE"]:
		var f = File.new()
		var exists = f.file_exists(resource_path)
		var size = -1
		var digest = ""
		if exists and f.open(resource_path, File.READ) == OK:
			var data = f.get_buffer(f.get_len())
			size = data.size()
			var ctx = HashingContext.new()
			ctx.start(HashingContext.HASH_SHA256)
			ctx.update(data)
			digest = ctx.finish().hex_encode()
			f.close()
		print(resource_path, " exists=", exists, " bytes=", size, " sha256=", digest)
	var source_file = File.new()
	var source_path = "res://scripts/BackendConfigManager.gd"
	assert(source_file.file_exists(source_path))
	assert(source_file.open(source_path, File.READ) == OK)
	var source = source_file.get_as_text()
	source_file.close()
	for token in ["gemma4:e2b", "gemma4:e4b", "_ollama_installer_step", "https://ollama.com/install.sh"]:
		assert(source.find(token) >= 0)
		print("BACKEND_PCK_TOKEN=", token)
	quit()
