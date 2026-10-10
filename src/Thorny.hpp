#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <unordered_map>

#include "ThornyEvents.hpp"

namespace thorny {

// Visual / behavioural states Thorny can be in. Transitions are driven by
// Thorny::onEvent and by the idle timer, never by pointer-chasing game code.
enum class ThornyState {
    Idle,
    Flying,
    Happy,
    Sad,
    Excited,
    Sleeping,
    Talking,
};

char const* toString(ThornyState state);

// The little spike character itself. It is a plain CCNode container so that
// movement, the idle "bob" and one-shot reactions never fight over the same
// transform:
//   this       -> position (movement)
//   m_bobNode  -> idle bob (scale / rotation)
//   m_sprite   -> one-shot reaction transforms
class Thorny : public cocos2d::CCNode {
public:
    static Thorny* create();

    bool init() override;
    void onEnter() override;
    void onExit() override;

    // Called by the manager with the freshly detected event + context.
    void onEvent(ThornyEvent event, ThornyContext const& context);

    // Show (or hide) the speech bubble with the given text. Empty text hides it.
    void say(std::string const& text);
    void hideBubble();

    // Settings were changed at runtime; re-apply what can be applied live.
    void applySettings();

    ThornyState state() const { return m_state; }

    // Tell Thorny the player did something, resetting the idle/sleep timer.
    void notifyActivity();

private:
    Thorny() = default;

    enum : int {
        kTagMove = 100,
        kTagBob = 101,
        kTagReaction = 102,
        kTagBubble = 103,
    };

    // -- lifecycle helpers --
    void buildSprite();
    void buildBubble();
    void refreshScreenBounds();
    void positionAtSpawn();
    void scheduleNextHop();

    // -- scheduled callbacks --
    void updateMovement(float dt);
    void updateChatter(float dt);

    // -- movement / animation --
    void startMovement();
    void stopMovement();
    void startBob();
    void setTexture(std::string const& name);
    void playReaction(ThornyState state);
    void setState(ThornyState state);

    // -- bubble --
    void layoutBubble(std::string const& text);

    cocos2d::CCNode* m_bobNode = nullptr;
    cocos2d::CCSprite* m_sprite = nullptr;
    cocos2d::CCLayer* m_bubble = nullptr;
    cocos2d::extension::CCScale9Sprite* m_bubbleBackground = nullptr;
    cocos2d::CCLabelBMFont* m_bubbleLabel = nullptr;
    cocos2d::CCSprite* m_bubbleTail = nullptr;

    // Cached textures so we never hit the texture cache during gameplay.
    std::unordered_map<int, cocos2d::CCTexture2D*> m_textures = {
        { 0, nullptr }, { 1, nullptr }, { 2, nullptr },
        { 3, nullptr }, { 4, nullptr }, { 5, nullptr },
    };

    ThornyState m_state = ThornyState::Idle;

    // Movement bounds in the parent's coordinate space.
    cocos2d::CCRect m_bounds = cocos2d::CCRectZero;

    bool m_movementStarted = false;
    bool m_pendingDestination = false;
    bool m_isSleeping = false;

    // Chatter timing.
    float m_chatterAccumulator = 0.0f;
    float m_nextChatter = 25.0f;
};

} // namespace thorny
