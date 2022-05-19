#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//

std::string trim(const std::string& s)
{
	std::string line = s;

	// Replacing tabs with a space.
	std::replace(line.begin(), line.end(), '\t', ' ');
	// Replacing carriage returns with a space.
	std::replace(line.begin(), line.end(), '\r', ' ');

	// Removing leading spaces.
	line.erase(line.begin(), std::find_if(line.begin(), line.end(), std::bind1st(std::not_equal_to<char>(), ' ')));
	// Removing trailing spaces.
	line.erase(std::find_if(line.rbegin(), line.rend(), std::bind1st(std::not_equal_to<char>(), ' ')).base(), line.end());

	// Removing duplicate spaces.
	auto it = std::unique(line.begin(), line.end(),
	            [](char const &lhs, char const &rhs) {
	                return (lhs == rhs) && (lhs == ' ');
	            });
	line.erase(it, line.end());

	return line;
}

//

bool generateHierarchy(json& glTF, size_t nodeIndex, std::string& data, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line.rfind("ROOT", 0) == 0)
		{
			size_t childNodeIndex = glTF["nodes"].size();
			glTF["scenes"][0]["nodes"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			glTF["nodes"][childNodeIndex]["name"] = line.substr(line.rfind(" ") + 1);

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, data, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line.rfind("JOINT", 0) == 0)
		{
			size_t childNodeIndex = glTF["nodes"].size();
			if (!glTF["nodes"][nodeIndex].contains("children"))
			{
				glTF["nodes"][nodeIndex]["children"] = json::array();
			}
			glTF["nodes"][nodeIndex]["children"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			glTF["nodes"][childNodeIndex]["name"] = line.substr(line.rfind(" ") + 1);

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, data, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line.rfind("End Site", 0) == 0)
		{
			size_t childNodeIndex = glTF["nodes"].size();
			if (!glTF["nodes"][nodeIndex].contains("children"))
			{
				glTF["nodes"][nodeIndex]["children"] = json::array();
			}
			glTF["nodes"][nodeIndex]["children"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			// Leaf has no name, so generate one from the parent node.
			glTF["nodes"][childNodeIndex]["name"] = glTF["nodes"][nodeIndex]["name"].get<std::string>() + " End";

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, data, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line.rfind("{", 0) == 0)
		{
			offset++;

			// Do nothing
		}
		else if (line.rfind("}", 0) == 0)
		{
			offset++;

			// Leave node
			return true;
		}
		else
		{
			offset++;
			printf("Unknown '%s'\n", line.c_str());
		}
	}

	return true;
}

bool generateMotion(json& glTF, std::string& data, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		offset++;

		// ToDo: Implement.
	}

	return true;
}

bool generate(json& glTF, std::string& data, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line == "HIERARCHY")
		{
			offset++;
			if (!generateHierarchy(glTF, 0, data, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line == "MOTION")
		{
			offset++;
			if (!generateMotion(glTF, data, offset, bvhLines))
			{
				return false;
			}
		}
		else
		{
			offset++;
			printf("Unknown '%s'\n", line.c_str());
		}
	}

	return true;
}

//

void gatherLines(std::vector<std::string>& bvhLines, const std::string& bvh)
{
	std::stringstream stringStream(bvh);
	std::string line;

	while(std::getline(stringStream, line, '\n'))
	{
		line = trim(line);

		bvhLines.push_back(line);
	}
}

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
    // BVH loading
    //

    std::string bvhContent;
	if (!loadFile(bvhContent, bvhFilename))
	{
		printf("Error: Could not load BVH file '%s'\n", bvhFilename.c_str());

		return -1;
	}

	printf("Info: Loaded BVH '%s'\n", bvhFilename.c_str());

	std::vector<std::string> bvhLines;
	gatherLines(bvhLines, bvhContent);

    //
    // glTF setup
    //

	std::string data;

    json glTF = json::object();
    glTF["asset"] = json::object();
    glTF["asset"]["version"] = "2.0";

    glTF["scenes"] = json::array();
    glTF["scenes"].push_back(json::object());
    glTF["scenes"][0]["children"] = json::array();

    glTF["nodes"] = json::array();

    //
    // BVH to glTF
    //

    size_t offset = 0;
    if (!generate(glTF, data, offset, bvhLines))
    {
    	printf("Error: Could not convert BVH to glTF\n");

    	return -1;
    }

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
