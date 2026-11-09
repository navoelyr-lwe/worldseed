/* Worldseed Engine — articulated creature body (reference biomechanics). */
#include "ws_creature.h"

#include <math.h>
#include <string.h>

/* Smallest positive length treated as zero, to avoid dividing by a degenerate
 * effective mass or normalising a null vector. */
#define WS_EPS 1e-12

void ws_cbody_make_box(ws_cbody *body, double half_len, double half_thick,
                       double half_width, double density, const uint8_t color[3])
{
    if (body == NULL) {
        return;
    }
    double len = 2.0 * half_len;
    double thick = 2.0 * half_thick;
    double width = 2.0 * half_width;
    double mass = density * len * thick * width;
    memset(body, 0, sizeof(*body));
    body->mass = mass;
    body->inv_mass = (mass > 0.0) ? 1.0 / mass : 0.0;
    /* Solid box inertia about the link centre, z axis: m*(len^2 + thick^2)/12. */
    body->inertia = mass * (len * len + thick * thick) / 12.0;
    body->inv_inertia = (body->inertia > 0.0) ? 1.0 / body->inertia : 0.0;
    body->half_len = half_len;
    body->half_thick = half_thick;
    body->half_width = half_width;
    if (color != NULL) {
        body->color[0] = color[0];
        body->color[1] = color[1];
        body->color[2] = color[2];
    }
}

ws_result ws_creature_init(ws_creature *c, double dt, double gravity,
                           int solver_iterations, double position_bias,
                           ws_cbody *bodies, uint32_t body_capacity,
                           ws_cjoint *joints, uint32_t joint_capacity,
                           ws_cfoot *feet, uint32_t foot_capacity)
{
    if (c == NULL || bodies == NULL || joints == NULL || feet == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || solver_iterations < 1 || position_bias < 0.0 ||
        position_bias > 1.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(c, 0, sizeof(*c));
    c->bodies = bodies;
    c->body_capacity = body_capacity;
    c->joints = joints;
    c->joint_capacity = joint_capacity;
    c->feet = feet;
    c->foot_capacity = foot_capacity;
    c->dt = dt;
    c->gravity = gravity;
    c->solver_iterations = solver_iterations;
    c->position_bias = position_bias;
    return WS_OK;
}

ws_result ws_creature_add_body(ws_creature *c, const ws_cbody *body,
                               uint32_t *out_index)
{
    if (c == NULL || body == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (c->body_count >= c->body_capacity) {
        return WS_ERR_CAPACITY;
    }
    ws_cbody copy = *body;
    copy.fx = 0.0;
    copy.fy = 0.0;
    copy.torque = 0.0;
    c->bodies[c->body_count] = copy;
    if (out_index != NULL) {
        *out_index = c->body_count;
    }
    c->body_count++;
    return WS_OK;
}

ws_result ws_creature_add_joint(ws_creature *c, const ws_cjoint *joint)
{
    if (c == NULL || joint == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (c->joint_count >= c->joint_capacity) {
        return WS_ERR_CAPACITY;
    }
    if (joint->a < 0 || joint->b < 0 ||
        (uint32_t)joint->a >= c->body_count ||
        (uint32_t)joint->b >= c->body_count) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    ws_cjoint copy = *joint;
    copy.rest_rel = c->bodies[joint->b].angle - c->bodies[joint->a].angle;
    c->joints[c->joint_count] = copy;
    c->joint_count++;
    return WS_OK;
}

ws_result ws_creature_add_foot(ws_creature *c, const ws_cfoot *foot)
{
    if (c == NULL || foot == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (c->foot_count >= c->foot_capacity) {
        return WS_ERR_CAPACITY;
    }
    if (foot->body < 0 || (uint32_t)foot->body >= c->body_count) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    c->feet[c->foot_count] = *foot;
    c->foot_count++;
    return WS_OK;
}

void ws_creature_set_ground(ws_creature *c, ws_profile_fn fn, void *ctx)
{
    if (c == NULL) {
        return;
    }
    c->ground = fn;
    c->ground_ctx = ctx;
}

void ws_cbody_point(const ws_cbody *b, double lx, double ly,
                    double *out_x, double *out_y)
{
    double ca = cos(b->angle);
    double sa = sin(b->angle);
    *out_x = b->px + ca * lx - sa * ly;
    *out_y = b->py + sa * lx + ca * ly;
}

/* Velocity of a body's local point, split into linear and angular parts. */
static void body_point_velocity(const ws_cbody *b, double lx, double ly,
                                double *out_vx, double *out_vy)
{
    double ca = cos(b->angle);
    double sa = sin(b->angle);
    double rx = ca * lx - sa * ly;
    double ry = sa * lx + ca * ly;
    *out_vx = b->vx - b->av * ry;
    *out_vy = b->vy + b->av * rx;
}

/* Apply an impulse P at local point (lx, ly) to a body. */
static void apply_impulse_at(ws_cbody *b, double lx, double ly,
                             double px, double py)
{
    if (b->inv_mass <= 0.0 && b->inv_inertia <= 0.0) {
        return;
    }
    b->vx += b->inv_mass * px;
    b->vy += b->inv_mass * py;
    double ca = cos(b->angle);
    double sa = sin(b->angle);
    double rx = ca * lx - sa * ly;
    double ry = sa * lx + ca * ly;
    b->av += b->inv_inertia * (rx * py - ry * px);
}

/* Ground contact detection: build the contact list for this step. */
static void build_contacts(ws_creature *c)
{
    c->contact_count = 0;
    if (c->ground == NULL) {
        return;
    }
    const double band = 1.0e-3; /* contact band, m */
    for (uint32_t i = 0; i < c->foot_count; ++i) {
        const ws_cfoot *f = &c->feet[i];
        const ws_cbody *b = &c->bodies[f->body];
        double wx, wy;
        ws_cbody_point(b, f->lx, f->ly, &wx, &wy);
        double gy = c->ground(c->ground_ctx, wx);
        double penetration = gy - wy;
        if (penetration < -band) {
            continue;
        }
        if (c->contact_count >= WS_CREATURE_MAX_CONTACTS) {
            break;
        }
        /* Surface normal from the local slope: tangent (1, dh/dx). */
        double h = 0.05;
        double slope = (c->ground(c->ground_ctx, wx + h) -
                        c->ground(c->ground_ctx, wx - h)) / (2.0 * h);
        double nlen = sqrt(slope * slope + 1.0);
        ws_ccontact *ct = &c->contacts[c->contact_count++];
        memset(ct, 0, sizeof(*ct));
        ct->body = f->body;
        ct->lx = f->lx;
        ct->ly = f->ly;
        ct->nx = -slope / nlen;
        ct->ny = 1.0 / nlen;
        ct->penetration = penetration > 0.0 ? penetration : 0.0;
        ct->friction = f->friction;
    }
}

static void solve_joint_point(ws_creature *c, ws_cjoint *j)
{
    ws_cbody *a = &c->bodies[j->a];
    ws_cbody *b = &c->bodies[j->b];
    double ax, ay, bx, by;
    ws_cbody_point(a, j->lax, j->lay, &ax, &ay);
    ws_cbody_point(b, j->lbx, j->lby, &bx, &by);
    double ex = bx - ax;
    double ey = by - ay;

    double ca_a = cos(a->angle), sa_a = sin(a->angle);
    double rax = ca_a * j->lax - sa_a * j->lay;
    double ray = sa_a * j->lax + ca_a * j->lay;
    double ca_b = cos(b->angle), sa_b = sin(b->angle);
    double rbx = ca_b * j->lbx - sa_b * j->lby;
    double rby = sa_b * j->lbx + ca_b * j->lby;

    double avx, avy, bvx, bvy;
    body_point_velocity(a, j->lax, j->lay, &avx, &avy);
    body_point_velocity(b, j->lbx, j->lby, &bvx, &bvy);
    double vrx = bvx - avx;
    double vry = bvy - avy;

    /* Effective mass K = Ka + Kb, each m_inv*I + i_inv*[[ry^2,-rx*ry],[-rx*ry,rx^2]]. */
    double k11 = a->inv_mass + b->inv_mass + a->inv_inertia * ray * ray +
                 b->inv_inertia * rby * rby;
    double k12 = -a->inv_inertia * rax * ray - b->inv_inertia * rbx * rby;
    double k22 = a->inv_mass + b->inv_mass + a->inv_inertia * rax * rax +
                 b->inv_inertia * rbx * rbx;
    double det = k11 * k22 - k12 * k12;
    if (fabs(det) < WS_EPS) {
        return;
    }
    double inv_det = 1.0 / det;
    double bias = c->position_bias / c->dt;
    double tx = -bias * ex;
    double ty = -bias * ey;
    double rx = tx - vrx;
    double ry = ty - vry;
    double px = inv_det * (k22 * rx - k12 * ry);
    double py = inv_det * (-k12 * rx + k11 * ry);

    apply_impulse_at(b, j->lbx, j->lby, px, py);
    apply_impulse_at(a, j->lax, j->lay, -px, -py);
}

/* Angular joint-limit constraint: one-sided on the relative body angle. */
static void solve_joint_limit(ws_creature *c, ws_cjoint *j)
{
    ws_cbody *a = &c->bodies[j->a];
    ws_cbody *b = &c->bodies[j->b];
    double q = (b->angle - a->angle) - j->rest_rel;
    double qd = b->av - a->av;
    double kang = a->inv_inertia + b->inv_inertia;
    if (kang < WS_EPS) {
        return;
    }
    double bias = c->position_bias / c->dt;
    double target = 0.0;
    int active = 0;
    if (q < j->min_angle) {
        target = bias * (j->min_angle - q); /* push q up */
        active = 1;
    } else if (q > j->max_angle) {
        target = -bias * (q - j->max_angle); /* push q down */
        active = 1;
    }
    if (!active) {
        return;
    }
    double lambda = (target - qd) / kang;
    /* One-sided: only correct toward the allowed range. */
    if ((q < j->min_angle && lambda < 0.0) ||
        (q > j->max_angle && lambda > 0.0)) {
        return;
    }
    b->av += b->inv_inertia * lambda;
    a->av -= a->inv_inertia * lambda;
}

static void solve_contact(ws_creature *c, ws_ccontact *ct)
{
    ws_cbody *b = &c->bodies[ct->body];
    double ca = cos(b->angle), sa = sin(b->angle);
    double rx = ca * ct->lx - sa * ct->ly;
    double ry = sa * ct->lx + ca * ct->ly;
    ct->rx = rx;
    ct->ry = ry;

    double vx, vy;
    body_point_velocity(b, ct->lx, ct->ly, &vx, &vy);
    double vn = vx * ct->nx + vy * ct->ny;
    double cross_n = rx * ct->ny - ry * ct->nx;
    double kn = b->inv_mass + b->inv_inertia * cross_n * cross_n;
    if (kn < WS_EPS) {
        return;
    }
    ct->kn = kn;
    double bias = c->position_bias / c->dt;
    double target = bias * ct->penetration; /* separate */
    double d = (target - vn) / kn;
    double new_l = ct->lambda_n + d;
    if (new_l < 0.0) {
        new_l = 0.0;
    }
    d = new_l - ct->lambda_n;
    ct->lambda_n = new_l;
    apply_impulse_at(b, ct->lx, ct->ly, d * ct->nx, d * ct->ny);

    /* Friction along the tangent, bounded by Coulomb. */
    double tx = -ct->ny;
    double ty = ct->nx;
    body_point_velocity(b, ct->lx, ct->ly, &vx, &vy);
    double vt = vx * tx + vy * ty;
    double cross_t = rx * ty - ry * tx;
    double kt = b->inv_mass + b->inv_inertia * cross_t * cross_t;
    if (kt < WS_EPS) {
        return;
    }
    ct->kt = kt;
    double dt_l = -vt / kt;
    double max_f = ct->friction * ct->lambda_n;
    double new_t = ct->lambda_t + dt_l;
    if (new_t > max_f) {
        new_t = max_f;
    } else if (new_t < -max_f) {
        new_t = -max_f;
    }
    dt_l = new_t - ct->lambda_t;
    ct->lambda_t = new_t;
    apply_impulse_at(b, ct->lx, ct->ly, dt_l * tx, dt_l * ty);
}

/* Non-penetration position pass: push a contacting foot out of the ground
 * along the contact normal, and carry the whole body with it. This keeps the
 * articulated pose from gradually sinking through the terrain, which a
 * velocity-level solver alone cannot guarantee. It moves positions only; it
 * never adds energy. */
static void correct_penetration(ws_creature *c)
{
    if (c->contact_count == 0) {
        return;
    }
    double dx = 0.0;
    double dy = 0.0;
    for (uint32_t i = 0; i < c->contact_count; ++i) {
        const ws_ccontact *ct = &c->contacts[i];
        if (ct->penetration <= 0.0) {
            continue;
        }
        double d = ct->penetration;
        dx += d * ct->nx;
        dy += d * ct->ny;
    }
    if (dx == 0.0 && dy == 0.0) {
        return;
    }
    for (uint32_t i = 0; i < c->body_count; ++i) {
        c->bodies[i].px += dx;
        c->bodies[i].py += dy;
    }
}

ws_result ws_creature_step(ws_creature *c)
{
    if (c == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const double dt = c->dt;

    /* 1. Forces: gravity plus muscle-equivalent motor torques. */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_cbody *b = &c->bodies[i];
        b->fx = 0.0;
        b->fy = -b->mass * c->gravity;
        b->torque = b->extra_torque;
    }
    for (uint32_t i = 0; i < c->joint_count; ++i) {
        ws_cjoint *j = &c->joints[i];
        if (!j->motor_enabled) {
            continue;
        }
        ws_cbody *a = &c->bodies[j->a];
        ws_cbody *b = &c->bodies[j->b];
        double q = (b->angle - a->angle) - j->rest_rel;
        double qd = b->av - a->av;
        double t = j->motor_kp * (j->motor_target - q) - j->motor_kd * qd;
        if (t > j->motor_max_torque) {
            t = j->motor_max_torque;
        } else if (t < -j->motor_max_torque) {
            t = -j->motor_max_torque;
        }
        /* Metabolic-equivalent cost: absolute mechanical work of the motor
         * (both positive and negative work cost energy in muscle). */
        c->motor_work += fabs(t * qd) * dt;
        c->motor_power = fabs(t * qd);
        b->torque += t;
        a->torque -= t;
    }

    /* 2. Integrate velocities (semi-implicit Euler). */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_cbody *b = &c->bodies[i];
        b->vx += b->fx * b->inv_mass * dt;
        b->vy += b->fy * b->inv_mass * dt;
        b->av += b->torque * b->inv_inertia * dt;
    }

    /* 3. Contacts at the current pose. */
    build_contacts(c);
    for (uint32_t i = 0; i < c->contact_count; ++i) {
        c->contacts[i].lambda_n = 0.0;
        c->contacts[i].lambda_t = 0.0;
    }

    /* 4. Sequential-impulse solve. */
    for (int it = 0; it < c->solver_iterations; ++it) {
        for (uint32_t i = 0; i < c->joint_count; ++i) {
            solve_joint_point(c, &c->joints[i]);
        }
        for (uint32_t i = 0; i < c->joint_count; ++i) {
            solve_joint_limit(c, &c->joints[i]);
        }
        for (uint32_t i = 0; i < c->contact_count; ++i) {
            solve_contact(c, &c->contacts[i]);
        }
    }

    /* 5. Integrate positions, then correct any residual foot penetration. */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_cbody *b = &c->bodies[i];
        b->px += b->vx * dt;
        b->py += b->vy * dt;
        b->angle += b->av * dt;
    }
    correct_penetration(c);
    return WS_OK;
}

ws_result ws_creature_advance(ws_creature *c, uint64_t steps)
{
    if (c == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    for (uint64_t s = 0; s < steps; ++s) {
        ws_result r = ws_creature_step(c);
        if (r != WS_OK) {
            return r;
        }
    }
    return WS_OK;
}

double ws_creature_total_mass(const ws_creature *c)
{
    double m = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        m += c->bodies[i].mass;
    }
    return m;
}

void ws_creature_center_of_mass(const ws_creature *c, double *out_x, double *out_y)
{
    double m = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_cbody *b = &c->bodies[i];
        m += b->mass;
        sx += b->mass * b->px;
        sy += b->mass * b->py;
    }
    if (m > 0.0) {
        *out_x = sx / m;
        *out_y = sy / m;
    } else {
        *out_x = 0.0;
        *out_y = 0.0;
    }
}

int ws_creature_support_interval(const ws_creature *c, double *out_min, double *out_max)
{
    double mn = 1.0e30;
    double mx = -1.0e30;
    int any = 0;
    for (uint32_t i = 0; i < c->contact_count; ++i) {
        const ws_ccontact *ct = &c->contacts[i];
        const ws_cbody *b = &c->bodies[ct->body];
        double wx, wy;
        ws_cbody_point(b, ct->lx, ct->ly, &wx, &wy);
        if (wx < mn) {
            mn = wx;
        }
        if (wx > mx) {
            mx = wx;
        }
        any = 1;
    }
    if (!any) {
        double cx, cy;
        ws_creature_center_of_mass(c, &cx, &cy);
        mn = cx;
        mx = cx;
    }
    *out_min = mn;
    *out_max = mx;
    return any;
}

double ws_creature_kinetic_energy(const ws_creature *c)
{
    double e = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_cbody *b = &c->bodies[i];
        e += 0.5 * b->mass * (b->vx * b->vx + b->vy * b->vy);
        e += 0.5 * b->inertia * b->av * b->av;
    }
    return e;
}
