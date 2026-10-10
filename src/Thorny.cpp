#include "Thorny.hpp"

#include "ThornyDialogue.hpp"
#include "ThornyManager.hpp"
#include "ThornySettings.hpp"

#include <Geode/utils/cocos.hpp>

#include <random>

using namespace geode::prelude;

namespace thorny {

namespace {

constexpr float kThornyScale = 1.0f;

// How far from the screen edges Thorny must stay.
constexpr float kPaddingX = 72.0f;
constexpr float kPaddingY = 82.0f;

std::mt19937& rng() {
    static std::mt19937 engine{ std::random_device{}() };
    return engine;
}

float randomRange(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng());
}

int randomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng());
}

} // namespace

char const* toString(ThornyState state) {
    switch (state) {
        case ThornyState::Idle: return "Idle";
        case ThornyState::Flying: return "Flying";
        case ThornyState::Happy: return "Happy";
        case ThornyState::Sad: return "Sad";
        case ThornyState::Excited: return "Excited";
        case ThornyState::Sleeping: return "Sleeping";
        case ThornyState::Talking: return "Talking";
        default: return "Unknown";
    }
}

Thorny* Thorny::create() {
    auto ret = new Thorny();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool Thorny::init() {
    if (!CCNode::init()) {
        return false;
    }

    this->setID("thorny-spike"_spr);
    this->setZOrder(1000);
    // Thorny is a plain CCNode, so it never participates in touch handling and
    // can never block a menu button.

    m_bobNode = CCNode::create();
    m_bobNode->setID("thorny-bob"_spr);
    this->addChild(m_bobNode);

    buildSprite();
    buildBubble();

    return true;
}

void Thorny::buildSprite() {
    // Load textures once. addImage() caches, so this is cheap and idempotent.
    auto cache = CCTextureCache::sharedTextureCache();
    m_textures[0] = cache->addImage("thorny_idle_001.png"_spr, false);
    m_textures[1] = cache->addImage("thorny_happy_001.png"_spr, false);
    m_textures[2] = cache->addImage("thorny_sad_001.png"_spr, false);
    m_textures[3] = cache->addImage("thorny_angry_001.png"_spr, false);
    m_textures[4] = cache->addImage("thorny_sleep_001.png"_spr, false);
    m_textures[5] = cache->addImage("thorny_surprised_001.png"_spr, false);

    if (m_textures[0]) {
        m_sprite = CCSprite::createWithTexture(m_textures[0]);
    }
    if (!m_sprite) {
        m_sprite = CCSprite::create("thorny_idle_001.png"_spr);
    }
    if (m_sprite) {
        m_sprite->setScale(kThornyScale);
        m_bobNode->addChild(m_sprite);
    } else {
        log::error("[Thorny] Could not load the Thorny sprite!");
    }
}

void Thorny::buildBubble() {
    m_bubble = CCLayer::create();
    m_bubble->setID("thorny-bubble"_spr);
    m_bubble->setVisible(false);
    this->addChild(m_bubble);

    auto bubbleTexture = CCTextureCache::sharedTextureCache()->addImage("thorny_bubble_001.png"_spr, false);
    if (bubbleTexture) {
        auto size = bubbleTexture->getContentSize();
        // 9-slice with ~14px corners so the bubble stretches for any text.
        cocos2d::CCRect capInsets(14.0f, 14.0f, size.width - 28.0f, size.height - 28.0f);
        m_bubbleBackground = cocos2d::extension::CCScale9Sprite::create(capInsets, "thorny_bubble_001.png"_spr);
    }
    if (!m_bubbleBackground) {
        m_bubbleBackground = cocos2d::extension::CCScale9Sprite::create("thorny_bubble_001.png"_spr);
    }
    if (m_bubbleBackground) {
        m_bubbleBackground->setID("thorny-bubble-bg"_spr);
        m_bubble->addChild(m_bubbleBackground, 1);
    }

    // Geode turns ThornyFont.ttf into a bitmap font at build time. It carries
    // both Latin and Cyrillic glyphs, so both languages render correctly.
    m_bubbleLabel = CCLabelBMFont::create("", "ThornyFont.fnt"_spr);
    m_bubble->addChild(m_bubbleLabel, 2);

    auto tailTexture = CCTextureCache::sharedTextureCache()->addImage("thorny_tail_001.png"_spr, false);
    if (tailTexture) {
        m_bubbleTail = CCSprite::createWithTexture(tailTexture);
    }
    if (!m_bubbleTail) {
        m_bubbleTail = CCSprite::create("thorny_tail_001.png"_spr);
    }
    if (m_bubbleTail) {
        m_bubbleTail->setID("thorny-bubble-tail"_spr);
        m_bubble->addChild(m_bubbleTail, 0);
    }
}

void Thorny::onEnter() {
    CCNode::onEnter();
    this->schedule(schedule_selector(Thorny::updateMovement), 0.0f);
    this->schedule(schedule_selector(Thorny::updateChatter), 1.0f);

    refreshScreenBounds();
    startBob();
    startMovement();
}

void Thorny::onExit() {
    // Stop every callback before we leave, so nothing fires on a dead node.
    this->unscheduleAllSelectors();
    if (m_sprite) {
        m_sprite->stopAllActions();
    }
    if (m_bobNode) {
        m_bobNode->stopAllActions();
    }
    if (m_bubble) {
        m_bubble->stopAllActions();
    }
    CCNode::onExit();
}

// --------------------------------------------------------------------------- movement

void Thorny::refreshScreenBounds() {
    auto* director = CCDirector::get();
    if (!director) {
        return;
    }
    auto size = director->getVisibleSize();
    auto origin = director->getVisibleOrigin();

    // Build the bounds in the parent's coordinate space, so this works for any
    // resolution / safe area without hard-coded numbers.
    float minX = origin.x + kPaddingX;
    float maxX = origin.x + size.width - kPaddingX;
    float minY = origin.y + kPaddingY;
    float maxY = origin.y + size.height - kPaddingY;

    if (auto* parent = this->getParent()) {
        auto parentOrigin = parent->convertToWorldSpace(cocos2d::CCPointZero);
        minX -= parentOrigin.x;
        maxX -= parentOrigin.x;
        minY -= parentOrigin.y;
        maxY -= parentOrigin.y;
    }

    if (maxX <= minX) {
        maxX = minX + 1.0f;
    }
    if (maxY <= minY) {
        maxY = minY + 1.0f;
    }

    m_bounds = cocos2d::CCRect(minX, minY, maxX - minX, maxY - minY);
}

void Thorny::chooseNextDestination() {
    if (m_bounds.size.width <= 0.0f) {
        return;
    }

    float duration = randomRange(4.5f, 9.0f);
    float targetX = randomRange(m_bounds.origin.x, m_bounds.origin.x + m_bounds.size.width);
    float targetY = randomRange(m_bounds.origin.y, m_bounds.origin.y + m_bounds.size.height);

    // Occasionally dash quickly, drift slowly, or hover in place.
    int roll = randomInt(0, 9);
    CCActionInterval* move = nullptr;
    if (roll == 0) {
        duration *= 0.35f;
        move = CCMoveTo::create(duration, cocos2d::CCPoint(targetX, targetY));
    } else if (roll == 1) {
        // Hover: aim for our own position.
        move = CCMoveTo::create(duration, this->getPosition());
    } else if (roll <= 3) {
        duration *= 1.4f;
        move = CCMoveTo::create(duration, cocos2d::CCPoint(targetX, targetY));
    } else {
        move = CCMoveTo::create(duration, cocos2d::CCPoint(targetX, targetY));
    }

    auto eased = CCEaseSineInOut::create(move);
    auto done = CallFuncExt::create([this]() {
        // Re-check bounds each hop in case the scene size changed.
        refreshScreenBounds();
        chooseNextDestination();
    });
    this->stopActionByTag(kTagMove);
    auto sequence = CCSequence::create(eased, done, nullptr);
    sequence->setTag(kTagMove);
    this->runAction(sequence);
}

void Thorny::startMovement() {
    if (m_movementStarted) {
        return;
    }
    if (!ThornySettings::get().movementEnabled()) {
        return;
    }
    m_movementStarted = true;
    refreshScreenBounds();

    if (m_bounds.size.width > 0.0f) {
        float startX = randomRange(m_bounds.origin.x, m_bounds.origin.x + m_bounds.size.width);
        float startY = randomRange(m_bounds.origin.y, m_bounds.origin.y + m_bounds.size.height);
        this->setPosition(startX, startY);
    }
    setState(ThornyState::Flying);
    chooseNextDestination();
}

void Thorny::stopMovement() {
    m_movementStarted = false;
    this->stopActionByTag(kTagMove);
}

void Thorny::updateMovement(float dt) {
    (void)dt;
    if (!ThornySettings::get().movementEnabled()) {
        if (m_movementStarted) {
            stopMovement();
        }
        return;
    }
    if (!m_movementStarted) {
        startMovement();
    }
}

// --------------------------------------------------------------------------- animations

void Thorny::startBob() {
    if (!m_bobNode) {
        return;
    }
    m_bobNode->stopAllActions();
    m_bobNode->setPosition(cocos2d::CCPointZero);

    auto up = CCMoveBy::create(0.9f, cocos2d::CCPoint(0.0f, 6.0f));
    auto down = CCMoveBy::create(0.9f, cocos2d::CCPoint(0.0f, -6.0f));
    auto bob = CCSequence::create(CCEaseSineInOut::create(up), CCEaseSineInOut::create(down), nullptr);
    m_bobNode->runAction(CCRepeatForever::create(bob));
}

void Thorny::setTexture(std::string const& name) {
    if (!m_sprite) {
        return;
    }
    auto texture = CCTextureCache::sharedTextureCache()->addImage(name.c_str(), false);
    if (texture) {
        m_sprite->setTexture(texture);
    }
}

void Thorny::setState(ThornyState state) {
    m_state = state;

    switch (state) {
        case ThornyState::Idle:
        case ThornyState::Flying:
            setTexture("thorny_idle_001.png"_spr);
            break;
        case ThornyState::Sleeping:
            setTexture("thorny_sleep_001.png"_spr);
            break;
        case ThornyState::Talking:
            setTexture("thorny_surprised_001.png"_spr);
            break;
        case ThornyState::Happy:
            setTexture("thorny_happy_001.png"_spr);
            break;
        case ThornyState::Sad:
            setTexture("thorny_sad_001.png"_spr);
            break;
        case ThornyState::Excited:
            setTexture("thorny_angry_001.png"_spr);
            break;
    }
}

void Thorny::playReaction(ThornyState state) {
    if (!m_sprite) {
        return;
    }
    m_sprite->stopActionByTag(kTagReaction);

    CCFiniteTimeAction* reaction = nullptr;

    switch (state) {
        case ThornyState::Happy: {
            // Two quick hops with a full spin.
            auto jumpUp = CCMoveBy::create(0.16f, cocos2d::CCPoint(0.0f, 22.0f));
            auto jumpDown = CCMoveBy::create(0.16f, cocos2d::CCPoint(0.0f, -22.0f));
            auto hop = CCSequence::create(jumpUp, jumpDown, nullptr);
            auto spinBack = CCRotateBy::create(0.01f, -720.0f);
            reaction = CCSequence::create(
                CCRepeat::create(hop, 2),
                CCRotateBy::create(0.5f, 720.0f),
                spinBack,
                nullptr
            );
            break;
        }
        case ThornyState::Excited: {
            // Sharp shake + fast spin.
            auto left = CCRotateBy::create(0.05f, -18.0f);
            auto right = CCRotateBy::create(0.05f, 18.0f);
            auto shake = CCSequence::create(left, right, nullptr);
            auto spinBack = CCRotateBy::create(0.01f, -540.0f);
            reaction = CCSequence::create(
                CCRepeat::create(shake, 4),
                CCRotateBy::create(0.5f, 540.0f),
                spinBack,
                nullptr
            );
            break;
        }
        case ThornyState::Sad: {
            // Sink a little, pause, then rise.
            auto sink = CCMoveBy::create(0.5f, cocos2d::CCPoint(0.0f, -10.0f));
            auto rise = CCMoveBy::create(0.8f, cocos2d::CCPoint(0.0f, 10.0f));
            reaction = CCSequence::create(
                CCEaseSineOut::create(sink),
                CCDelayTime::create(0.4f),
                CCEaseSineInOut::create(rise),
                nullptr
            );
            break;
        }
        case ThornyState::Sleeping: {
            auto shrink = CCScaleTo::create(0.6f, 0.85f * kThornyScale);
            auto back = CCScaleTo::create(0.6f, kThornyScale);
            reaction = CCSequence::create(
                CCEaseSineInOut::create(shrink),
                CCDelayTime::create(0.3f),
                CCEaseSineInOut::create(back),
                nullptr
            );
            break;
        }
        default: {
            auto pulse = CCScaleTo::create(0.12f, 1.15f * kThornyScale);
            auto back = CCScaleTo::create(0.14f, kThornyScale);
            reaction = CCSequence::create(pulse, back, nullptr);
            break;
        }
    }

    if (!reaction) {
        return;
    }

    auto finished = state;
    auto sequence = CCSequence::create(
        reaction,
        CallFuncExt::create([this, finished]() {
            if (m_state == finished) {
                setState(ThornyState::Flying);
            }
        }),
        nullptr
    );
    sequence->setTag(kTagReaction);
    m_sprite->runAction(sequence);
}

// --------------------------------------------------------------------------- chatter

void Thorny::notifyActivity() {
    m_chatterAccumulator = 0.0f;
    if (m_isSleeping) {
        m_isSleeping = false;
        setState(ThornyState::Flying);
        startBob();
    }
}

void Thorny::updateChatter(float dt) {
    auto settings = ThornySettings::get();
    m_chatterAccumulator += dt;

    // Sleep after a long stretch of nothing happening.
    constexpr float kSleepAfter = 70.0f;
    if (!m_isSleeping && m_chatterAccumulator >= kSleepAfter) {
        m_isSleeping = true;
        setState(ThornyState::Sleeping);
        playReaction(ThornyState::Sleeping);
        if (settings.dialogueEnabled()) {
            auto line = ThornyManager::get().dialogueForEvent(ThornyEvent::FellAsleep);
            if (!line.empty()) {
                say(line);
            }
        }
        m_chatterAccumulator = 0.0f;
        m_nextChatter = randomRange(45.0f, 80.0f);
        return;
    }

    if (m_isSleeping) {
        return;
    }

    if (!settings.dialogueEnabled() || m_chatterAccumulator < m_nextChatter) {
        return;
    }

    m_chatterAccumulator = 0.0f;
    m_nextChatter = randomRange(28.0f, 55.0f) * settings.frequencyMultiplier();

    auto line = ThornyManager::get().dialogueForEvent(ThornyEvent::StayedIdle);
    if (!line.empty()) {
        say(line);
    }
}

// --------------------------------------------------------------------------- bubble

void Thorny::layoutBubble(std::string const& text) {
    if (!m_bubble || !m_bubbleLabel || !m_bubbleBackground) {
        return;
    }

    m_bubbleLabel->setString(text.c_str());
    m_bubbleLabel->limitLabelWidth(180.0f, 0.65f, 0.35f);
    auto labelSize = m_bubbleLabel->getScaledContentSize();

    constexpr float padX = 12.0f;
    constexpr float padY = 9.0f;
    float bubbleWidth = labelSize.width + padX * 2.0f;
    float bubbleHeight = labelSize.height + padY * 2.0f;

    m_bubbleBackground->setContentSize(cocos2d::CCSize(bubbleWidth, bubbleHeight));
    m_bubbleBackground->setPosition(cocos2d::CCPointZero);
    m_bubbleLabel->setPosition(cocos2d::CCPointZero);

    constexpr float tailHeight = 10.0f;
    if (m_bubbleTail) {
        // Tail sits flush under the bubble rectangle.
        m_bubbleTail->setPosition(cocos2d::CCPoint(
            0.0f, -bubbleHeight / 2.0f - tailHeight / 2.0f + 1.0f));
    }

    float halfHeight = bubbleHeight / 2.0f + tailHeight;
    float bubbleY = 30.0f + halfHeight + 4.0f;
    bool flip = false;

    if (auto* director = CCDirector::get()) {
        auto worldOrigin = this->convertToWorldSpace(cocos2d::CCPointZero);
        auto visibleSize = director->getVisibleSize();
        auto visibleOrigin = director->getVisibleOrigin();
        if (worldOrigin.y + bubbleY + halfHeight > visibleOrigin.y + visibleSize.height) {
            flip = true;
        }
    }

    if (flip) {
        // Mirror vertically so the tail points down at Thorny.
        m_bubble->setScaleY(-1.0f);
        m_bubble->setPosition(cocos2d::CCPoint(0.0f, -30.0f - halfHeight - 4.0f));
        if (m_bubbleTail) {
            m_bubbleTail->setPosition(cocos2d::CCPoint(0.0f, bubbleHeight / 2.0f + tailHeight / 2.0f - 1.0f));
        }
    } else {
        m_bubble->setScaleY(1.0f);
        m_bubble->setPosition(cocos2d::CCPoint(0.0f, bubbleY));
        if (m_bubbleTail) {
            m_bubbleTail->setPosition(cocos2d::CCPoint(0.0f, -bubbleHeight / 2.0f - tailHeight / 2.0f + 1.0f));
        }
    }

    // Keep the bubble from leaving the screen horizontally.
    if (auto* director = CCDirector::get()) {
        auto worldOrigin = this->convertToWorldSpace(cocos2d::CCPointZero);
        auto visibleSize = director->getVisibleSize();
        auto visibleOrigin = director->getVisibleOrigin();
        float halfWidth = bubbleWidth / 2.0f;
        float offsetX = 0.0f;
        if (worldOrigin.x - halfWidth < visibleOrigin.x) {
            offsetX = visibleOrigin.x - (worldOrigin.x - halfWidth);
        } else if (worldOrigin.x + halfWidth > visibleOrigin.x + visibleSize.width) {
            offsetX = (visibleOrigin.x + visibleSize.width) - (worldOrigin.x + halfWidth);
        }
        auto pos = m_bubble->getPosition();
        m_bubble->setPosition(cocos2d::CCPoint(pos.x + offsetX, pos.y));
    }
}

void Thorny::say(std::string const& text) {
    if (text.empty() || !m_bubble) {
        return;
    }
    if (!ThornySettings::get().dialogueEnabled()) {
        return;
    }

    layoutBubble(text);
    m_bubble->setVisible(true);
    m_bubble->stopAllActions();
    m_bubble->setScale(0.6f);
    m_bubble->runAction(CCEaseBackOut::create(CCScaleTo::create(0.18f, 1.0f)));

    scheduleBubbleHide();
    setState(ThornyState::Talking);
}

void Thorny::scheduleBubbleHide() {
    if (!m_bubble) {
        return;
    }
    m_bubble->stopAllActions();
    m_bubble->runAction(CCSequence::create(
        CCDelayTime::create(4.5f),
        CCEaseSineIn::create(CCScaleTo::create(0.25f, 0.6f)),
        CallFuncExt::create([this]() {
            hideBubble();
            if (m_state == ThornyState::Talking) {
                setState(m_isSleeping ? ThornyState::Sleeping : ThornyState::Flying);
            }
        }),
        nullptr
    ));
}

void Thorny::hideBubble() {
    if (m_bubble) {
        m_bubble->setVisible(false);
    }
}

// --------------------------------------------------------------------------- events

void Thorny::onEvent(ThornyEvent event, ThornyContext const& context) {
    // Any real event counts as activity: it pushes the idle/sleep timer back.
    notifyActivity();

    switch (event) {
        case ThornyEvent::CompletedLevel:
            setState(ThornyState::Happy);
            playReaction(ThornyState::Happy);
            break;
        case ThornyEvent::FailedLevel:
            setState(ThornyState::Sad);
            playReaction(ThornyState::Sad);
            break;
        case ThornyEvent::StartedLevel:
        case ThornyEvent::ExitedEditor:
        case ThornyEvent::EditedLevel:
            setState(ThornyState::Excited);
            playReaction(ThornyState::Excited);
            break;
        default:
            break;
    }

    if (ThornySettings::get().dialogueEnabled()) {
        auto line = ThornyManager::get().dialogueForEvent(event, &context);
        if (!line.empty()) {
            say(line);
        }
    }
}

void Thorny::applySettings() {
    if (ThornySettings::get().movementEnabled()) {
        if (!m_movementStarted) {
            startMovement();
        }
    } else {
        stopMovement();
    }

    if (!ThornySettings::get().dialogueEnabled()) {
        hideBubble();
    }
}

} // namespace thorny
