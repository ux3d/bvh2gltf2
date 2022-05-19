#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool loadFile(std::string& output, const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	file.seekg(0);

	output.resize(fileSize);

	file.read(output.data(), fileSize);
	file.close();

	return true;
}

bool saveFile(const std::string& output, const std::string& filename)
{
	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}

	file.write(output.data(), output.size());
	file.close();

	return true;
}

int main(int argc, char *argv[])
{
	std::string bvhFilename = "Example1.bvh";

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 && (i + 1 < argc))
        {
        	bvhFilename = argv[i + 1];
        }
    }

    //
    // BVH loading and parsing
    //

    std::string bvhContent;
	if (!loadFile(bvhContent, bvhFilename))
	{
		printf("Error: Could not load BVH file '%s'\n", bvhFilename.c_str());

		return -1;
	}

	printf("Info: Loaded BVH '%s'\n", bvhFilename.c_str());

	// ToDO: Implement.

    //
    // glTF setup and conversion
    //

    json glTF = json::object();
	std::string data;

	// ToDO: Implement.

    //
	// Saving everything
	//

    std::string saveBinaryName = "untitled.bin";

	if (!saveFile(data, saveBinaryName))
	{
		printf("Error: Could not save generated bin file '%s'\n", saveBinaryName.c_str());

		return -1;
	}

    std::string saveGltfName = "untitled.gltf";

	if (!saveFile(glTF.dump(3), saveGltfName))
	{
		printf("Error: Could not save generated glTF file '%s'\n", saveGltfName.c_str());

		return -1;
	}

	printf("Info: Saved glTF '%s'\n", saveGltfName.c_str());

	return 0;
}
