#ifndef __TR_CAMERA__
#define __TR_CAMERA__

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "window.hpp"

class TRCamera
{
    public:
        struct MoveKeyBinding
        {
            MoveKeyBinding()
            {
            }

            MoveKeyBinding(int forward, int backward, int left, int right)
            : mForward(forward),
              mBackward(backward),
              mLeft(left),
              mRight(right)
            {
            }

            int mForward = SDL_SCANCODE_UP;
            int mBackward = SDL_SCANCODE_DOWN;
            int mLeft = SDL_SCANCODE_LEFT;
            int mRight = SDL_SCANCODE_RIGHT;
        };

        TRCamera()
        {
            updateVectors();
        }

        void setPosition(const glm::vec3 &position)
        {
            mPosition = position;
        }

        void setWorldUp(const glm::vec3 &up)
        {
            if (glm::length(up) > 1e-6f)
                mWorldUp = glm::normalize(up);
            updateVectors();
        }

        void setYawPitch(float yawDeg, float pitchDeg)
        {
            mYawDeg = yawDeg;
            mPitchDeg = clampPitch(pitchDeg);
            updateVectors();
        }

        void setPerspective(float fovDeg, float aspect, float nearPlane, float farPlane)
        {
            mFovDeg = fovDeg;
            mAspect = aspect;
            mNearPlane = nearPlane;
            mFarPlane = farPlane;
        }

        void setAspect(float aspect)
        {
            mAspect = aspect;
        }

        void setMoveSpeed(float speed)
        {
            mMoveSpeed = speed;
        }

        void setLookSensitivity(float sensitivity)
        {
            mLookSensitivity = sensitivity;
        }

        void setMoveKeyBinding(const MoveKeyBinding &binding)
        {
            mMoveKeyBinding = binding;
        }

        const MoveKeyBinding &getMoveKeyBinding() const
        {
            return mMoveKeyBinding;
        }

        const glm::vec3 &getPosition() const
        {
            return mPosition;
        }

        const glm::vec3 &getForward() const
        {
            return mForward;
        }

        const glm::vec3 &getRight() const
        {
            return mRight;
        }

        const glm::vec3 &getUp() const
        {
            return mUp;
        }

        float getYawDeg() const
        {
            return mYawDeg;
        }

        float getPitchDeg() const
        {
            return mPitchDeg;
        }

        void handleMouseDelta(float dx, float dy)
        {
            mYawDeg += dx * mLookSensitivity;
            mPitchDeg = clampPitch(mPitchDeg - dy * mLookSensitivity);
            updateVectors();
        }

        void moveLocal(float forward, float right, float up, float dt)
        {
            glm::vec3 movement = mForward * forward + mRight * right + mWorldUp * up;
            float len = glm::length(movement);
            if (len <= 1e-6f)
                return;

            movement /= len;
            mPosition += movement * mMoveSpeed * dt;
        }

        void moveForward(float distance)
        {
            mPosition += mForward * distance;
        }

        void translateWorld(const glm::vec3 &delta)
        {
            mPosition += delta;
        }

        void updateFromWindow(const TRWindow &window, float dt)
        {
            float forward = 0.0f;
            float right = 0.0f;

            if (window.isKeyPressed(mMoveKeyBinding.mForward))
                forward += 1.0f;
            if (window.isKeyPressed(mMoveKeyBinding.mBackward))
                forward -= 1.0f;
            if (window.isKeyPressed(mMoveKeyBinding.mRight))
                right += 1.0f;
            if (window.isKeyPressed(mMoveKeyBinding.mLeft))
                right -= 1.0f;

            moveLocal(forward, right, 0.0f, dt);
        }

        void lookAt(const glm::vec3 &target)
        {
            glm::vec3 forward = target - mPosition;
            if (glm::length(forward) <= 1e-6f)
                return;

            forward = glm::normalize(forward);
            mPitchDeg = glm::degrees(asinf(glm::clamp(forward.y, -1.0f, 1.0f)));
            mYawDeg = glm::degrees(atan2f(forward.z, forward.x));
            updateVectors();
        }

        void orbitAround(const glm::vec3 &target, float yawDeltaDeg, float pitchDeltaDeg = 0.0f)
        {
            glm::vec3 offset = mPosition - target;
            if (glm::length(offset) <= 1e-6f)
                return;

            glm::mat4 transform(1.0f);
            if (std::abs(yawDeltaDeg) > 1e-6f)
                transform = glm::rotate(transform, glm::radians(yawDeltaDeg), mWorldUp);

            if (std::abs(pitchDeltaDeg) > 1e-6f)
            {
                glm::vec3 pitchAxis = glm::cross(offset, mWorldUp);
                if (glm::length(pitchAxis) > 1e-6f)
                    transform = glm::rotate(transform, glm::radians(pitchDeltaDeg), glm::normalize(pitchAxis)) * transform;
            }

            mPosition = target + glm::vec3(transform * glm::vec4(offset, 1.0f));
            lookAt(target);
        }

        glm::mat4 getViewMatrix() const
        {
            return glm::lookAt(mPosition, mPosition + mForward, mUp);
        }

        glm::mat4 getProjectionMatrix() const
        {
            return glm::perspective(glm::radians(mFovDeg), mAspect, mNearPlane, mFarPlane);
        }

    private:
        glm::vec3 mPosition = glm::vec3(0.0f, 0.0f, 3.0f);
        glm::vec3 mWorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 mForward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 mRight = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 mUp = glm::vec3(0.0f, 1.0f, 0.0f);
        float mYawDeg = -90.0f;
        float mPitchDeg = 0.0f;
        float mMoveSpeed = 8.0f;
        float mLookSensitivity = 0.15f;
        float mFovDeg = 60.0f;
        float mAspect = 16.0f / 9.0f;
        float mNearPlane = 0.1f;
        float mFarPlane = 100.0f;
        MoveKeyBinding mMoveKeyBinding;

        static float clampPitch(float pitchDeg)
        {
            return glm::clamp(pitchDeg, -89.0f, 89.0f);
        }

        void updateVectors()
        {
            float yaw = glm::radians(mYawDeg);
            float pitch = glm::radians(mPitchDeg);

            glm::vec3 forward;
            forward.x = cosf(yaw) * cosf(pitch);
            forward.y = sinf(pitch);
            forward.z = sinf(yaw) * cosf(pitch);
            mForward = glm::normalize(forward);

            mRight = glm::cross(mForward, mWorldUp);
            if (glm::length(mRight) <= 1e-6f)
                mRight = glm::vec3(1.0f, 0.0f, 0.0f);
            else
                mRight = glm::normalize(mRight);

            mUp = glm::cross(mRight, mForward);
            if (glm::length(mUp) <= 1e-6f)
                mUp = mWorldUp;
            else
                mUp = glm::normalize(mUp);
        }
};

#endif
