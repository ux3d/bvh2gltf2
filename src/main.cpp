#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//

struct FrameData {
	std::vector<float> values;
};

struct MotionData {
	size_t frames = 0;
	float frameTime = 0.0f;
	std::vector<FrameData> frameDatas;
};

//

std::string trim(const std::string& s)
{
	std::string line = s;

	// Replacing tabs with a space.
	std::replace(line.begin(), line.end(), '\t', ' ');
	// Replacing carriage returns with a space.
	std::replace(line.begin(), line.end(), '\r', ' ');

	// Removing leading spaces.
	line.erase(line.begin(), std::find_if(line.begin(), line.end(), std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' ')));
	// Removing trailing spaces.
	line.erase(std::find_if(line.rbegin(), line.rend(), std::bind(std::not_equal_to<char>(),  std::placeholders::_1, ' ')).base(), line.end());

	// Removing duplicate spaces.
	auto it = std::unique(line.begin(), line.end(),
	            [](char const &lhs, char const &rhs) {
	                return (lhs == rhs) && (lhs == ' ');
	            });
	line.erase(it, line.end());

	return line;
}

//

bool generateHierarchy(json& glTF, size_t nodeIndex, std::vector<uint8_t>& byteData, const glm::mat4& parentMatrix, size_t& offset, const std::vector<std::string>& bvhLines)
{
	glm::mat4 currentMatrix = parentMatrix;

	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line.rfind("ROOT", 0) == 0)
		{
			size_t childNodeIndex = glTF["nodes"].size();
			glTF["scenes"][0]["nodes"].push_back(childNodeIndex);

			glTF["skins"][0]["joints"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			glTF["nodes"][childNodeIndex]["name"] = line.substr(line.rfind(" ") + 1);

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, offset, bvhLines))
			{
				return false;
			}

			// Leave HIERARCHY section
			return true;
		}
		else if (line.rfind("JOINT", 0) == 0)
		{
			size_t childNodeIndex = glTF["nodes"].size();
			if (!glTF["nodes"][nodeIndex].contains("children"))
			{
				glTF["nodes"][nodeIndex]["children"] = json::array();
			}
			glTF["nodes"][nodeIndex]["children"].push_back(childNodeIndex);

			glTF["skins"][0]["joints"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			glTF["nodes"][childNodeIndex]["name"] = line.substr(line.rfind(" ") + 1);

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, offset, bvhLines))
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

			glTF["skins"][0]["joints"].push_back(childNodeIndex);

			glTF["nodes"].push_back(json::object());
			// Leaf has no name, so generate one from the parent node.
			glTF["nodes"][childNodeIndex]["name"] = glTF["nodes"][nodeIndex]["name"].get<std::string>() + " End";

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line.rfind("{", 0) == 0)
		{
			offset++;

			printf("Entering node '%s'\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str());
		}
		else if (line.rfind("OFFSET", 0) == 0)
		{
			offset++;

			char buffer[7];
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;

			sscanf(line.c_str(), "%s %f %f %f", buffer, &x, &y, &z);

			glTF["nodes"][nodeIndex]["translation"] = json::array();
			glTF["nodes"][nodeIndex]["translation"].push_back(x);
			glTF["nodes"][nodeIndex]["translation"].push_back(y);
			glTF["nodes"][nodeIndex]["translation"].push_back(z);

			currentMatrix = parentMatrix * glm::translate(glm::mat4(), glm::vec3(x, y, z));
		}
		else if (line.rfind("CHANNELS", 0) == 0)
		{
			offset++;

			// TODO: Implement.
			printf("Debug (HIERARCHY) '%s'\n", line.c_str());
		}
		else if (line.rfind("}", 0) == 0)
		{
			offset++;

			glm::mat4 inverseMatrix = glm::inverse(parentMatrix);

			size_t offset = byteData.size();
			byteData.resize(byteData.size() + 16 * sizeof(float));
			memcpy(byteData.data() + offset, glm::value_ptr(inverseMatrix), 16 * sizeof(float));

			// Leave node
			printf("Leaving node '%s'\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str());
			return true;
		}
		else
		{
			printf("Unknown (HIERARCHY) '%s'\n", line.c_str());
			return false;
		}
	}

	return true;
}

bool gatherSamples(MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		offset++;

		// TODO: Implement.
		printf("Debug (MOTION) '%s'\n", line.c_str());
	}

	return true;
}

bool generateMotion(MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line.rfind("Frames:", 0) == 0)
		{
			motionData.frames = (size_t)std::stoul(line.substr(line.rfind(" ") + 1));
			motionData.frameDatas.resize(motionData.frames);

			offset++;
		}
		else if (line.rfind("Frame Time:", 0) == 0)
		{
			motionData.frameTime = std::stof(line.substr(line.rfind(" ") + 1));

			offset++;
			if (!gatherSamples(motionData, offset, bvhLines))
			{
				return false;
			}

			// Leave MOTION section
			return true;
		}
		else
		{
			printf("Unknown (MOTION) '%s'\n", line.c_str());

			return false;
		}
	}

	return true;
}

bool generate(json& glTF, std::vector<uint8_t>& byteData, MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line == "HIERARCHY")
		{
			offset++;
			if (!generateHierarchy(glTF, 0, byteData, glm::mat4(1.0f), offset, bvhLines))
			{
				return false;
			}
		}
		else if (line == "MOTION")
		{
			offset++;
			if (!generateMotion(motionData, offset, bvhLines))
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

	std::string saveGltfName = "untitled.gltf";
	std::string saveBinaryName = "untitled.bin";

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

	std::vector<uint8_t> byteData;
	std::string data;

    json glTF = json::object();
    glTF["asset"] = json::object();
    glTF["asset"]["version"] = "2.0";

    glTF["scenes"] = json::array();
    glTF["scenes"].push_back(json::object());
    glTF["scenes"][0]["nodes"] = json::array();

    glTF["buffers"] = json::array();
    glTF["buffers"].push_back(json::object());
    glTF["buffers"][0]["uri"] = saveBinaryName;

    glTF["bufferViews"] = json::array();
    glTF["bufferViews"].push_back(json::object());
    glTF["bufferViews"][0]["buffer"] = 0;

    glTF["accessors"] = json::array();
    glTF["accessors"].push_back(json::object());
    glTF["accessors"][0]["componentType"] = 5126;
    glTF["accessors"][0]["type"] = "MAT4";

    glTF["skins"] = json::array();
    glTF["skins"].push_back(json::object());
    glTF["skins"][0]["joints"] = json::array();
    glTF["skins"][0]["inverseBindMatrices"] = 0;

    glTF["nodes"] = json::array();

    //
    // BVH to glTF
    //

    MotionData motionData;

    size_t offset = 0;
    if (!generate(glTF, byteData, motionData, offset, bvhLines))
    {
    	printf("Error: Could not convert BVH to glTF\n");

    	return -1;
    }

    data.resize(byteData.size());
    // Copy current content
    memcpy(data.data(), byteData.data(), byteData.size());

    glTF["buffers"][0]["byteLength"] = data.size();
    glTF["bufferViews"][0]["byteLength"] = data.size();
    glTF["accessors"][0]["count"] = glTF["nodes"].size();

    size_t nodeIndex = glTF["nodes"].size();

    glTF["scenes"][0]["nodes"].push_back(nodeIndex);

    glTF["nodes"].push_back(json::object());
    glTF["nodes"][nodeIndex]["name"] = "Mesh";
    glTF["nodes"][nodeIndex]["skin"] = 0;

    //
	// Saving everything
	//

	if (!saveFile(data, saveBinaryName))
	{
		printf("Error: Could not save generated bin file '%s'\n", saveBinaryName.c_str());

		return -1;
	}

	if (!saveFile(glTF.dump(3), saveGltfName))
	{
		printf("Error: Could not save generated glTF file '%s'\n", saveGltfName.c_str());

		return -1;
	}

	printf("Info: Saved glTF '%s'\n", saveGltfName.c_str());

	return 0;
}
