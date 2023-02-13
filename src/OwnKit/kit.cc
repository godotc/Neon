#include <GLX/OwnKit/kit.h>
#include <corecrt_math.h>
#include <gl/glew.h>

void ownkit::DrawCircle(glm::vec3 &&rgb, int steps, int radius)
{
    const float angle = PI * 2 / steps;

    float oldX = 0, oldY = 1 * radius;

    for (int i = 0; i <= steps; ++i)
    {
        float newX = radius * cos(angle * i);
        float newY = -radius * sin(angle * i);

        glColor3f(rgb.x, rgb.y, rgb.z);

        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0, 0);
        glVertex3f(oldX, oldY, 0);
        glVertex3f(newX, newY, 0);
        glEnd();

        oldX = newX;
        oldY = newY;
    }
}