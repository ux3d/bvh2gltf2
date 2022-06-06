#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//

struct NodeData {
	std::vector<std::string> positionChannels;
	std::vector<std::string> rotationChannels;
	std::vector<float> positionData;
	std::vector<float> rotationData;
};

struct HierarchyData {
	std::vector<NodeData> nodeDatas;
};

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

bool generateHierarchy(json& glTF, size_t nodeIndex, std::vector<uint8_t>& byteData, const glm::mat4& parentMatrix, HierarchyData& hierarchyData, size_t& offset, const std::vector<std::string>& bvhLines)
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

			hierarchyData.nodeDatas.push_back(NodeData());

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, hierarchyData, offset, bvhLines))
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

			hierarchyData.nodeDatas.push_back(NodeData());

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, hierarchyData, offset, bvhLines))
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

			hierarchyData.nodeDatas.push_back(NodeData());

			offset++;
			if (!generateHierarchy(glTF, childNodeIndex, byteData, currentMatrix, hierarchyData, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line.rfind("{", 0) == 0)
		{
			offset++;

			printf("Info: Entering node '%s'\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str());
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

			currentMatrix = parentMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));

			printf("Info: Node '%s' has offsets %f %f %f\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str(), x, y, z);
		}
		else if (line.rfind("CHANNELS", 0) == 0)
		{
			offset++;

			std::stringstream stringStream(line);
			std::string token;
			size_t currentToken = 0;
			while(std::getline(stringStream, token, ' '))
			{
				if (currentToken == 1)
				{
					printf("Info: Node '%s' has %s channels\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str(), token.c_str());
				}
				else if (currentToken >= 2)
				{
					if (token == "Xposition" || token == "Yposition" || token == "Zposition")
					{
						hierarchyData.nodeDatas[nodeIndex].positionChannels.push_back(token);
					}
					else if (token == "Xrotation" || token == "Yrotation" || token == "Zrotation")
					{
						hierarchyData.nodeDatas[nodeIndex].rotationChannels.push_back(token);
					}
					else
					{
						printf("Unknown (HIERARCHY) token '%s'\n", token.c_str());
						return false;
					}
				}

				currentToken++;
			}
		}
		else if (line.rfind("}", 0) == 0)
		{
			offset++;

			glm::mat4 inverseMatrix = glm::inverse(parentMatrix);

			size_t offset = byteData.size();
			byteData.resize(byteData.size() + 16 * sizeof(float));
			memcpy(byteData.data() + offset, glm::value_ptr(inverseMatrix), 16 * sizeof(float));

			// Leave node
			printf("Info: Leaving node '%s'\n", glTF["nodes"][nodeIndex]["name"].get<std::string>().c_str());
			return true;
		}
		else
		{
			printf("Error: Unknown in HIERARCHY '%s'\n", line.c_str());
			return false;
		}
	}

	return true;
}

bool gatherSamples(MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
{
	size_t currentFrame = 0;

	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		offset++;

		//

		std::stringstream stringStream(line);
		std::string token;
		while(std::getline(stringStream, token, ' '))
		{
			motionData.frameDatas[currentFrame].values.push_back(std::stof(token));
		}
		printf("Info: Frame %zu has %zu samples\n", currentFrame, motionData.frameDatas[currentFrame].values.size());
		currentFrame++;
	}

	return true;
}

bool generateMotion(HierarchyData& hierarchyData, MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
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
			printf("Error: Unknown in MOTION '%s'\n", line.c_str());

			return false;
		}
	}

	return true;
}

bool generate(json& glTF, std::vector<uint8_t>& byteData, HierarchyData& hierarchyData, MotionData& motionData, size_t& offset, const std::vector<std::string>& bvhLines)
{
	while (offset < bvhLines.size())
	{
		const std::string& line = bvhLines[offset];

		if (line == "HIERARCHY")
		{
			offset++;
			if (!generateHierarchy(glTF, 0, byteData, glm::mat4(1.0f), hierarchyData, offset, bvhLines))
			{
				return false;
			}
		}
		else if (line == "MOTION")
		{
			offset++;
			if (!generateMotion(hierarchyData, motionData, offset, bvhLines))
			{
				return false;
			}
		}
		else
		{
			offset++;
			printf("Error: Unknown '%s'\n", line.c_str());
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
    glTF["accessors"][0]["bufferView"] = 0;

    glTF["skins"] = json::array();
    glTF["skins"].push_back(json::object());
    glTF["skins"][0]["joints"] = json::array();
    glTF["skins"][0]["inverseBindMatrices"] = 0;

    glTF["nodes"] = json::array();

    glTF["animations"] = json::array();
    glTF["animations"].push_back(json::object());
    glTF["animations"][0]["samplers"] = json::array();
    glTF["animations"][0]["channels"] = json::array();

    //
    // BVH to glTF
    //

    HierarchyData hierarchyData;
    MotionData motionData;

    size_t offset = 0;
    if (!generate(glTF, byteData, hierarchyData, motionData, offset, bvhLines))
    {
    	printf("Error: Could not convert BVH to glTF\n");

    	return -1;
    }

    //

	size_t currentDataIndex = 0;

    // Sorting data per node, target position and rotation
    for (size_t currentFrameIndex = 0; currentFrameIndex < motionData.frames; currentFrameIndex++)
    {
    	currentDataIndex = 0;

    	for (auto& currentNode : hierarchyData.nodeDatas)
    	{
        	for (size_t p = 0; p < currentNode.positionChannels.size(); p++)
    		{
        		currentNode.positionData.push_back(motionData.frameDatas[currentFrameIndex].values[currentDataIndex]);
    			currentDataIndex++;
    		}
    		for (size_t r = 0; r < currentNode.rotationChannels.size(); r++)
    		{
    			currentNode.rotationData.push_back(motionData.frameDatas[currentFrameIndex].values[currentDataIndex]);
    			currentDataIndex++;
    		}
    	}
    }

    //

    size_t byteOffset = 0;

    data.resize(byteData.size());
    // Copy current content
    memcpy(data.data() + byteOffset, byteData.data(), byteData.size());

    glTF["bufferViews"][0]["byteLength"] = data.size();
    glTF["accessors"][0]["count"] = glTF["nodes"].size();

    byteOffset = data.size();

    //
    // Key frames
    //

    std::vector<float> keyframes(motionData.frames);
    for (size_t i = 0; i < motionData.frames; i++)
    {
    	keyframes[i] = motionData.frameTime * (float)i;
    }

	size_t bufferViewIndex = glTF["bufferViews"].size();

    glTF["bufferViews"].push_back(json::object());
    glTF["bufferViews"][bufferViewIndex]["buffer"] = 0;
    glTF["bufferViews"][bufferViewIndex]["byteOffset"] = byteOffset;
    glTF["bufferViews"][bufferViewIndex]["byteLength"] = motionData.frames * sizeof(float);

	size_t accessorIndex = glTF["accessors"].size();

    glTF["accessors"].push_back(json::object());
    glTF["accessors"][accessorIndex]["bufferView"] = bufferViewIndex;
    glTF["accessors"][accessorIndex]["componentType"] = 5126;
    glTF["accessors"][accessorIndex]["count"] = motionData.frames;
    glTF["accessors"][accessorIndex]["type"] = "SCALAR";

    glTF["accessors"][accessorIndex]["min"] = json::array();
    glTF["accessors"][accessorIndex]["min"].push_back(keyframes.front());

    glTF["accessors"][accessorIndex]["max"] = json::array();
    glTF["accessors"][accessorIndex]["max"].push_back(keyframes.back());

    data.resize(data.size() + keyframes.size() * sizeof(float));
    memcpy(data.data() + byteOffset, keyframes.data(), keyframes.size() * sizeof(float));

    byteOffset = data.size();

    //

    size_t inputAccessorIndex = accessorIndex;

	//

    size_t animationSamplerIndex;
    size_t animationChannelIndex;

    // Generate animations, as we now do have all the data sorted out.
	for (size_t currentNodeIndex = 0; currentNodeIndex < hierarchyData.nodeDatas.size(); currentNodeIndex++)
	{
		auto& currentNode = hierarchyData.nodeDatas[currentNodeIndex];

		if (currentNode.positionChannels.size() > 0)
		{
			bufferViewIndex = glTF["bufferViews"].size();

		    glTF["bufferViews"].push_back(json::object());
		    glTF["bufferViews"][bufferViewIndex]["buffer"] = 0;
		    glTF["bufferViews"][bufferViewIndex]["byteOffset"] = byteOffset;
		    glTF["bufferViews"][bufferViewIndex]["byteLength"] = motionData.frames * 3 * sizeof(float);

		    //

			accessorIndex = glTF["accessors"].size();

		    glTF["accessors"].push_back(json::object());
		    glTF["accessors"][accessorIndex]["bufferView"] = bufferViewIndex;
		    glTF["accessors"][accessorIndex]["componentType"] = 5126;
		    glTF["accessors"][accessorIndex]["count"] = motionData.frames;
		    glTF["accessors"][accessorIndex]["type"] = "VEC3";

		    //

		    std::vector<float> finalPositionData(motionData.frames * 3);

		    for (size_t currentFrameIndex = 0; currentFrameIndex < motionData.frames; currentFrameIndex++)
		    {
		    	for (size_t i = 0; i < currentNode.positionChannels.size(); i++)
		    	{
		    		if (currentNode.positionChannels[i] == "Xposition")
		    		{
		    			finalPositionData[currentFrameIndex * 3 + 0] = currentNode.positionData[currentFrameIndex * currentNode.positionChannels.size() + i];
		    		}
		    		else if (currentNode.positionChannels[i] == "Yposition")
		    		{
		    			finalPositionData[currentFrameIndex * 3 + 1] = currentNode.positionData[currentFrameIndex * currentNode.positionChannels.size() + i];
		    		}
		    		else if (currentNode.positionChannels[i] == "Zposition")
		    		{
		    			finalPositionData[currentFrameIndex * 3 + 2] = currentNode.positionData[currentFrameIndex * currentNode.positionChannels.size() + i];
		    		}
		    	}
		    }

		    data.resize(data.size() + finalPositionData.size() * sizeof(float));
		    memcpy(data.data() + byteOffset, finalPositionData.data(), finalPositionData.size() * sizeof(float));

		    byteOffset = data.size();

		    //

		    animationSamplerIndex = glTF["animations"][0]["samplers"].size();
		    animationChannelIndex = glTF["animations"][0]["channels"].size();

		    glTF["animations"][0]["samplers"].push_back(json::object());
		    glTF["animations"][0]["channels"].push_back(json::object());

		    glTF["animations"][0]["samplers"][animationSamplerIndex]["input"] = inputAccessorIndex;
		    glTF["animations"][0]["samplers"][animationSamplerIndex]["interpolation"] = "LINEAR";
		    glTF["animations"][0]["samplers"][animationSamplerIndex]["output"] = accessorIndex;

		    glTF["animations"][0]["channels"][animationChannelIndex]["sampler"] = animationSamplerIndex;
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"] = json::object();
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"]["path"] = "translation";
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"]["node"] = currentNodeIndex;
		}
		if (currentNode.rotationChannels.size() > 0)
		{
			bufferViewIndex = glTF["bufferViews"].size();

		    glTF["bufferViews"].push_back(json::object());
		    glTF["bufferViews"][bufferViewIndex]["buffer"] = 0;
		    glTF["bufferViews"][bufferViewIndex]["byteOffset"] = byteOffset;
		    glTF["bufferViews"][bufferViewIndex]["byteLength"] = motionData.frames * 4 * sizeof(float);

		    //

			accessorIndex = glTF["accessors"].size();

		    glTF["accessors"].push_back(json::object());
		    glTF["accessors"][accessorIndex]["bufferView"] = bufferViewIndex;
		    glTF["accessors"][accessorIndex]["componentType"] = 5126;
		    glTF["accessors"][accessorIndex]["count"] = motionData.frames;
		    glTF["accessors"][accessorIndex]["type"] = "VEC4";

		    //

		    std::vector<float> finalRotationData(motionData.frames * 4);

		    for (size_t currentFrameIndex = 0; currentFrameIndex < motionData.frames; currentFrameIndex++)
		    {
		    	glm::mat4 matrix(1.0f);

		    	for (size_t i = 0; i < currentNode.positionChannels.size(); i++)
		    	{
		    		if (currentNode.positionChannels[i] == "Xrotation")
		    		{
		    			matrix = glm::rotate(matrix, currentNode.rotationData[currentFrameIndex * currentNode.rotationChannels.size() + i], glm::vec3(1.0f, 0.0f, 0.0f));
		    		}
		    		else if (currentNode.positionChannels[i] == "Yrotation")
		    		{
		    			matrix = glm::rotate(matrix, currentNode.rotationData[currentFrameIndex * currentNode.rotationChannels.size() + i], glm::vec3(0.0f, 1.0f, 0.0f));
		    		}
		    		else if (currentNode.positionChannels[i] == "Zrotation")
		    		{
		    			matrix = glm::rotate(matrix, currentNode.rotationData[currentFrameIndex * currentNode.rotationChannels.size() + i], glm::vec3(0.0f, 0.0f, 1.0f));
		    		}
		    	}

		    	glm::quat rotation = glm::toQuat(matrix);

		    	finalRotationData[currentFrameIndex * 4 + 0] = rotation.x;
		    	finalRotationData[currentFrameIndex * 4 + 1] = rotation.y;
		    	finalRotationData[currentFrameIndex * 4 + 2] = rotation.z;
		    	finalRotationData[currentFrameIndex * 4 + 3] = rotation.w;
		    }

		    data.resize(data.size() + finalRotationData.size() * sizeof(float));
		    memcpy(data.data() + byteOffset, finalRotationData.data(), finalRotationData.size() * sizeof(float));

		    byteOffset = data.size();

		    //

		    animationChannelIndex = glTF["animations"][0]["channels"].size();
		    animationSamplerIndex = glTF["animations"][0]["samplers"].size();

		    glTF["animations"][0]["samplers"].push_back(json::object());
		    glTF["animations"][0]["channels"].push_back(json::object());

		    glTF["animations"][0]["samplers"][animationSamplerIndex]["input"] = inputAccessorIndex;
		    glTF["animations"][0]["samplers"][animationSamplerIndex]["interpolation"] = "LINEAR";
		    glTF["animations"][0]["samplers"][animationSamplerIndex]["output"] = accessorIndex;

		    glTF["animations"][0]["channels"][animationChannelIndex]["sampler"] = animationSamplerIndex;
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"] = json::object();
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"]["path"] = "rotation";
		    glTF["animations"][0]["channels"][animationChannelIndex]["target"]["node"] = currentNodeIndex;
		}
	}

    //

    size_t nodeIndex = glTF["nodes"].size();

    glTF["scenes"][0]["nodes"].push_back(nodeIndex);

    glTF["nodes"].push_back(json::object());
    glTF["nodes"][nodeIndex]["name"] = "Mesh";
    glTF["nodes"][nodeIndex]["skin"] = 0;

    glTF["buffers"][0]["byteLength"] = data.size();

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
