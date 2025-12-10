#pragma once
#include <memory>
#include <vector>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

/**
 * @brief 繧ｿ繧､繝医Ν逕ｻ髱｢逕ｨ轤取浤繧ｨ繝輔ぉ繧ｯ繝医け繝ｩ繧ｹ・・PS迚茨ｼ・
 * 
 * 繧ｿ繧､繝医Ν逕ｻ髱｢縺ｧ逕ｻ髱｢螂･縺ｫ蜷代°縺｣縺ｦ騾｣邯夂噪縺ｫ轤取浤縺檎ｫ九■荳翫′繧区ｼ泌・繧堤ｮ｡逅・＠縺ｾ縺吶・
 * 譁ｰNPS繧ｷ繧ｹ繝・Β繧剃ｽｿ逕ｨ縺励◆螳溯｣・・
 */
class TitleFireEffect
{
public:
    /**
     * @brief 繧ｨ繝輔ぉ繧ｯ繝医す繧ｹ繝・Β縺ｮ蛻晄悄蛹・
     */
    void Initialize();
    
    /**
     * @brief 繧ｨ繝輔ぉ繧ｯ繝医・譖ｴ譁ｰ
     * @param cameraPos 繧ｫ繝｡繝ｩ縺ｮ迴ｾ蝨ｨ菴咲ｽｮ
     */
    void Update(const Vector3& cameraPos);
    
    /**
     * @brief 轤取浤縺ｮ逋ｺ逕・
     * @param position 逋ｺ逕溷渕貅紋ｽ咲ｽｮ
     */
    void EmitFire(const Vector3& position);

private:
    std::unique_ptr<ParticleEmitter> fireEmitterRight_;  ///< 蜿ｳ蛛ｴ縺ｮ轤取浤繧ｨ繝溘ャ繧ｿ繝ｼ
    std::unique_ptr<ParticleEmitter> fireEmitterLeft_;   ///< 蟾ｦ蛛ｴ縺ｮ轤取浤繧ｨ繝溘ャ繧ｿ繝ｼ
    std::unique_ptr<ParticleEmitter> floorEmitter_;      ///< 蠎企擇繧ｨ繝輔ぉ繧ｯ繝医お繝溘ャ繧ｿ繝ｼ
    Vector3 floorPos_ = {};                         ///< 蠎企擇繧ｨ繝輔ぉ繧ｯ繝医・菴咲ｽｮ
    float lastFireZ_ = 0.0f;                        ///< 譛蠕後↓轤弱ｒ逋ｺ逕溘＆縺帙◆Z蠎ｧ讓・
    const float interval_ = 1.0f;                   ///< 轤守匱逕溘・譎る俣髢馴囈・育ｧ抵ｼ・
    const float laneOffset_ = 1.5f;                 ///< 蟾ｦ蜿ｳ縺ｮ繝ｬ繝ｼ繝ｳ繧ｪ繝輔そ繝・ヨ
    const float groundY_ = -0.5f;                   ///< 蝨ｰ髱｢縺ｮY蠎ｧ讓・
	float time_ = 0.0f;                             ///< 繧ｿ繧､繝槭・・育ｧ抵ｼ・
	const std::string fireTexturePath_ = "./Resources/gradation.png";
	bool firstUpdate_ = true;
};
