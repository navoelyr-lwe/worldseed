/* Worldseed Engine — three-dimensional articulated body (P13). */
#include "ws_body3d.h"

#include <math.h>
#include <string.h>

#define WS_EPS 1e-12

/* ---------- small 3x3 helpers (local; ws_math exposes only mul_vec3) ---------- */

static ws_mat3 mat3_mul(ws_mat3 a, ws_mat3 b)
{
    ws_mat3 out;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) {
                s += a.m[k * 3 + row] * b.m[col * 3 + k];
            }
            out.m[col * 3 + row] = s;
        }
    }
    return out;
}

static ws_mat3 mat3_transpose(ws_mat3 a)
{
    ws_mat3 out;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            out.m[col * 3 + row] = a.m[row * 3 + col];
        }
    }
    return out;
}

static ws_mat3 mat3_add(ws_mat3 a, ws_mat3 b)
{
    ws_mat3 out;
    for (int i = 0; i < 9; ++i) {
        out.m[i] = a.m[i] + b.m[i];
    }
    return out;
}

/* Solve A x = b for a 3x3 A by Cramer's rule; returns 0 if singular. */
static int mat3_solve(ws_mat3 a, ws_vec3 b, ws_vec3 *out)
{
    double m0 = a.m[0], m1 = a.m[1], m2 = a.m[2];
    double m3 = a.m[3], m4 = a.m[4], m5 = a.m[5];
    double m6 = a.m[6], m7 = a.m[7], m8 = a.m[8];
    double det = m0 * (m4 * m8 - m5 * m7) - m3 * (m1 * m8 - m2 * m7) +
                 m6 * (m1 * m5 - m2 * m4);
    if (fabs(det) < WS_EPS) {
        return 0;
    }
    double inv = 1.0 / det;
    out->x = inv * (b.x * (m4 * m8 - m5 * m7) - m3 * (b.y * m8 - b.z * m7) +
                    m6 * (b.y * m5 - b.z * m4));
    out->y = inv * (m0 * (b.y * m8 - b.z * m7) - b.x * (m1 * m8 - m2 * m7) +
                    m6 * (m1 * b.z - m2 * b.y));
    out->z = inv * (m0 * (m4 * b.z - b.y * m7) - m3 * (m1 * b.z - b.y * m2) +
                    b.x * (m1 * m5 - m2 * m4));
    return 1;
}

/* Skew-symmetric matrix [r]x so that [r]x * v = r x v. */
static ws_mat3 skew(ws_vec3 r)
{
    ws_mat3 m;
    memset(&m, 0, sizeof(m));
    m.m[1] = -r.z; m.m[2] = r.y;
    m.m[3] = r.z;  m.m[5] = -r.x;
    m.m[6] = -r.y; m.m[7] = r.x;
    return m;
}

/* ---------- quaternion ---------- */

ws_quat ws_quat_make(double w, double x, double y, double z)
{
    ws_quat q = {w, x, y, z};
    return q;
}

ws_quat ws_quat_mul(ws_quat a, ws_quat b)
{
    return ws_quat_make(
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w);
}

ws_quat ws_quat_normalize(ws_quat q)
{
    double n = sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n < WS_EPS) {
        return ws_quat_make(1.0, 0.0, 0.0, 0.0);
    }
    double inv = 1.0 / n;
    return ws_quat_make(q.w * inv, q.x * inv, q.y * inv, q.z * inv);
}

ws_vec3 ws_quat_rotate(ws_quat q, ws_vec3 v)
{
    /* v' = v + 2 * q_vec x (q_vec x v + w v) */
    ws_vec3 u = ws_vec3_make(q.x, q.y, q.z);
    ws_vec3 t = ws_vec3_cross(u, v);
    t = ws_vec3_add(t, ws_vec3_scale(v, q.w));
    t = ws_vec3_cross(u, t);
    return ws_vec3_add(v, ws_vec3_scale(t, 2.0));
}

ws_mat3 ws_quat_to_mat3(ws_quat q)
{
    q = ws_quat_normalize(q);
    double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    ws_mat3 m;
    /* column-major: m[col*3 + row] */
    m.m[0] = 1.0 - 2.0 * (yy + zz);
    m.m[1] = 2.0 * (xy + wz);
    m.m[2] = 2.0 * (xz - wy);
    m.m[3] = 2.0 * (xy - wz);
    m.m[4] = 1.0 - 2.0 * (xx + zz);
    m.m[5] = 2.0 * (yz + wx);
    m.m[6] = 2.0 * (xz + wy);
    m.m[7] = 2.0 * (yz - wx);
    m.m[8] = 1.0 - 2.0 * (xx + yy);
    return m;
}

ws_quat ws_quat_integrate(ws_quat q, ws_vec3 w, double dt)
{
    /* dq = 0.5 * (0, w) * q ; q += dq * dt ; renormalise. */
    ws_quat wq = ws_quat_make(0.0, w.x, w.y, w.z);
    ws_quat dq = ws_quat_mul(wq, q);
    ws_quat out = ws_quat_make(q.w + 0.5 * dq.w * dt,
                               q.x + 0.5 * dq.x * dt,
                               q.y + 0.5 * dq.y * dt,
                               q.z + 0.5 * dq.z * dt);
    return ws_quat_normalize(out);
}

/* ---------- body construction ---------- */

void ws_body3_make_box(ws_body3 *body, double half_len, double half_thick,
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
    /* Solid box inertia about the centre, body frame (diagonal). */
    double ix = mass * (thick * thick + width * width) / 12.0;
    double iy = mass * (len * len + width * width) / 12.0;
    double iz = mass * (len * len + thick * thick) / 12.0;
    memset(&body->inertia_body, 0, sizeof(body->inertia_body));
    body->inertia_body.m[0] = ix;
    body->inertia_body.m[4] = iy;
    body->inertia_body.m[8] = iz;
    memset(&body->inv_inertia_body, 0, sizeof(body->inv_inertia_body));
    body->inv_inertia_body.m[0] = (ix > 0.0) ? 1.0 / ix : 0.0;
    body->inv_inertia_body.m[4] = (iy > 0.0) ? 1.0 / iy : 0.0;
    body->inv_inertia_body.m[8] = (iz > 0.0) ? 1.0 / iz : 0.0;
    body->half_len = half_len;
    body->half_thick = half_thick;
    body->half_width = half_width;
    body->orient = ws_quat_make(1.0, 0.0, 0.0, 0.0);
    if (color != NULL) {
        body->color[0] = color[0];
        body->color[1] = color[1];
        body->color[2] = color[2];
    }
}

ws_mat3 ws_body3_inv_inertia_world(const ws_body3 *b)
{
    ws_mat3 r = ws_quat_to_mat3(b->orient);
    /* I_world^-1 = R * I_body^-1 * R^T. */
    return mat3_mul(mat3_mul(r, b->inv_inertia_body), mat3_transpose(r));
}


/* ---------- lifecycle ---------- */

ws_result ws_body3d_init(ws_body3d *c, double dt, double gravity,
                         int solver_iterations, double position_bias,
                         ws_body3 *bodies, uint32_t body_capacity,
                         ws_joint3 *joints, uint32_t joint_capacity,
                         ws_foot3 *feet, uint32_t foot_capacity)
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

ws_result ws_body3d_add_body(ws_body3d *c, const ws_body3 *body,
                             uint32_t *out_index)
{
    if (c == NULL || body == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (c->body_count >= c->body_capacity) {
        return WS_ERR_CAPACITY;
    }
    ws_body3 copy = *body;
    copy.force = ws_vec3_make(0.0, 0.0, 0.0);
    copy.torque = ws_vec3_make(0.0, 0.0, 0.0);
    c->bodies[c->body_count] = copy;
    if (out_index != NULL) {
        *out_index = c->body_count;
    }
    c->body_count++;
    return WS_OK;
}

/* Hinge angle of joint j: the signed rotation of body b relative to body a
 * about the parent's hinge axis, measured from the assembly pose. */
double ws_joint3_hinge_angle(const ws_body3d *c, const ws_joint3 *j)
{
    const ws_body3 *a = &c->bodies[j->a];
    const ws_body3 *b = &c->bodies[j->b];
    ws_mat3 ra = ws_quat_to_mat3(a->orient);
    ws_mat3 rb = ws_quat_to_mat3(b->orient);
    /* Relative rotation R = Ra^T * Rb; the angle about axis k is atan2 of the
     * off-diagonal pair. */
    ws_mat3 rel = mat3_mul(mat3_transpose(ra), rb);
    double ang;
    if (j->hinge_axis == 0) {
        ang = atan2(rel.m[1 * 3 + 2], rel.m[1 * 3 + 1]);
    } else if (j->hinge_axis == 1) {
        ang = atan2(rel.m[2 * 3 + 0], rel.m[2 * 3 + 2]);
    } else {
        ang = atan2(rel.m[0 * 3 + 1], rel.m[0 * 3 + 0]);
    }
    return ang;
}

/* Hinge-axis unit vector in world space (the parent's local axis). */
static ws_vec3 joint_hinge_axis_world(const ws_body3d *c, const ws_joint3 *j)
{
    ws_vec3 axis = ws_vec3_make(0.0, 0.0, 0.0);
    if (j->hinge_axis == 0) {
        axis.x = 1.0;
    } else if (j->hinge_axis == 1) {
        axis.y = 1.0;
    } else {
        axis.z = 1.0;
    }
    return ws_quat_rotate(c->bodies[j->a].orient, axis);
}

ws_result ws_body3d_add_joint(ws_body3d *c, const ws_joint3 *joint)
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
    if (joint->hinge_axis < 0 || joint->hinge_axis > 2) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    ws_joint3 copy = *joint;
    c->joints[c->joint_count] = copy;
    c->joints[c->joint_count].rest_angle = ws_joint3_hinge_angle(c, &copy);
    c->joint_count++;
    return WS_OK;
}

ws_result ws_body3d_add_foot(ws_body3d *c, const ws_foot3 *foot)
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

void ws_body3d_set_ground(ws_body3d *c, ws_height_fn fn, void *ctx)
{
    if (c == NULL) {
        return;
    }
    c->ground = fn;
    c->ground_ctx = ctx;
}

ws_vec3 ws_body3_point(const ws_body3 *b, ws_vec3 local)
{
    return ws_vec3_add(b->pos, ws_quat_rotate(b->orient, local));
}

ws_vec3 ws_body3_point_velocity(const ws_body3 *b, ws_vec3 local)
{
    ws_vec3 r = ws_quat_rotate(b->orient, local);
    return ws_vec3_add(b->vel, ws_vec3_cross(b->omega, r));
}

/* ---------- impulses ---------- */

static void apply_impulse(ws_body3 *b, ws_vec3 r, ws_vec3 p)
{
    if (b->inv_mass <= 0.0 && b->inv_inertia_body.m[0] <= 0.0) {
        return;
    }
    b->vel = ws_vec3_add(b->vel, ws_vec3_scale(p, b->inv_mass));
    ws_mat3 iw = ws_body3_inv_inertia_world(b);
    ws_vec3 dw = ws_mat3_mul_vec3(iw, ws_vec3_cross(r, p));
    b->omega = ws_vec3_add(b->omega, dw);
}

/* ---------- contacts ---------- */

static void build_contacts(ws_body3d *c)
{
    c->contact_count = 0;
    if (c->ground == NULL) {
        return;
    }
    const double band = 1.0e-3;
    for (uint32_t i = 0; i < c->foot_count; ++i) {
        const ws_foot3 *f = &c->feet[i];
        const ws_body3 *b = &c->bodies[f->body];
        ws_vec3 w = ws_body3_point(b, f->local);
        double gy = c->ground(c->ground_ctx, w.x, w.z);
        double penetration = gy - w.y;
        if (penetration < -band) {
            continue;
        }
        if (c->contact_count >= WS_BODY3D_MAX_CONTACTS) {
            break;
        }
        double h = 0.05;
        double dhdx = (c->ground(c->ground_ctx, w.x + h, w.z) -
                       c->ground(c->ground_ctx, w.x - h, w.z)) / (2.0 * h);
        double dhdz = (c->ground(c->ground_ctx, w.x, w.z + h) -
                       c->ground(c->ground_ctx, w.x, w.z - h)) / (2.0 * h);
        ws_vec3 n = ws_vec3_normalize(ws_vec3_make(-dhdx, 1.0, -dhdz));
        ws_contact3 *ct = &c->contacts[c->contact_count++];
        memset(ct, 0, sizeof(*ct));
        ct->body = f->body;
        ct->local = f->local;
        ct->normal = n;
        ct->penetration = penetration > 0.0 ? penetration : 0.0;
        ct->friction = f->friction;
    }
}

/* Effective mass along direction `dir` at contact offset r (world). */
static double effective_mass(const ws_body3 *b, ws_vec3 r, ws_vec3 dir)
{
    ws_mat3 iw = ws_body3_inv_inertia_world(b);
    ws_vec3 rn = ws_vec3_cross(r, dir);
    ws_vec3 tmp = ws_mat3_mul_vec3(iw, rn);
    double angular = ws_vec3_dot(ws_vec3_cross(tmp, r), dir);
    return b->inv_mass + angular;
}

static void solve_contact(ws_body3d *c, ws_contact3 *ct)
{
    ws_body3 *b = &c->bodies[ct->body];
    ws_vec3 r = ws_quat_rotate(b->orient, ct->local);
    ct->r = r;

    ws_vec3 v = ws_body3_point_velocity(b, ct->local);

    /* Normal. */
    double vn = ws_vec3_dot(v, ct->normal);
    double kn = effective_mass(b, r, ct->normal);
    if (kn < WS_EPS) {
        return;
    }
    ct->kn = kn;
    double bias = c->position_bias / c->dt;
    double target = bias * ct->penetration;
    double d = (target - vn) / kn;
    double new_l = ct->lambda_n + d;
    if (new_l < 0.0) {
        new_l = 0.0;
    }
    d = new_l - ct->lambda_n;
    ct->lambda_n = new_l;
    apply_impulse(b, r, ws_vec3_scale(ct->normal, d));

    /* Two tangents: build an orthonormal basis around the normal. */
    ws_vec3 t1 = (fabs(ct->normal.y) < 0.9)
                     ? ws_vec3_normalize(ws_vec3_cross(ct->normal,
                                                       ws_vec3_make(0.0, 1.0, 0.0)))
                     : ws_vec3_normalize(ws_vec3_cross(ct->normal,
                                                       ws_vec3_make(1.0, 0.0, 0.0)));
    ws_vec3 t2 = ws_vec3_cross(ct->normal, t1);
    double max_f = ct->friction * ct->lambda_n;
    v = ws_body3_point_velocity(b, ct->local);

    double vt1 = ws_vec3_dot(v, t1);
    double k1 = effective_mass(b, r, t1);
    if (k1 > WS_EPS) {
        double dl = -vt1 / k1;
        double nt = ct->lambda_t1 + dl;
        if (nt > max_f) {
            nt = max_f;
        } else if (nt < -max_f) {
            nt = -max_f;
        }
        dl = nt - ct->lambda_t1;
        ct->lambda_t1 = nt;
        apply_impulse(b, r, ws_vec3_scale(t1, dl));
    }
    v = ws_body3_point_velocity(b, ct->local);
    double vt2 = ws_vec3_dot(v, t2);
    double k2 = effective_mass(b, r, t2);
    if (k2 > WS_EPS) {
        double dl = -vt2 / k2;
        double nt = ct->lambda_t2 + dl;
        if (nt > max_f) {
            nt = max_f;
        } else if (nt < -max_f) {
            nt = -max_f;
        }
        dl = nt - ct->lambda_t2;
        ct->lambda_t2 = nt;
        apply_impulse(b, r, ws_vec3_scale(t2, dl));
    }
}

/* ---------- joints ---------- */

static void solve_joint_point(ws_body3d *c, ws_joint3 *j)
{
    ws_body3 *a = &c->bodies[j->a];
    ws_body3 *b = &c->bodies[j->b];
    ws_vec3 wa = ws_body3_point(a, j->la);
    ws_vec3 wb = ws_body3_point(b, j->lb);
    ws_vec3 e = ws_vec3_sub(wb, wa);

    ws_vec3 ra = ws_quat_rotate(a->orient, j->la);
    ws_vec3 rb = ws_quat_rotate(b->orient, j->lb);
    ws_mat3 ia = ws_body3_inv_inertia_world(a);
    ws_mat3 ib = ws_body3_inv_inertia_world(b);
    ws_mat3 ska = skew(ra);
    ws_mat3 skb = skew(rb);
    /* K = (ma^-1 + mb^-1) I + [ra]x^T Ia [ra]x + [rb]x^T Ib [rb]x.
     * [ra]x^T Ia [ra]x = -[ra]x Ia [ra]x (since [r]x^T = -[r]x). */
    ws_mat3 k = mat3_add(mat3_mul(mat3_transpose(ska), mat3_mul(ia, ska)),
                         mat3_mul(mat3_transpose(skb), mat3_mul(ib, skb)));
    double lin = a->inv_mass + b->inv_mass;
    k.m[0] += lin;
    k.m[4] += lin;
    k.m[8] += lin;

    ws_vec3 va = ws_body3_point_velocity(a, j->la);
    ws_vec3 vb = ws_body3_point_velocity(b, j->lb);
    ws_vec3 vr = ws_vec3_sub(vb, va);
    double bias = c->position_bias / c->dt;
    ws_vec3 rhs = ws_vec3_sub(ws_vec3_scale(e, -bias), vr);
    ws_vec3 p;
    if (!mat3_solve(k, rhs, &p)) {
        return;
    }
    apply_impulse(b, rb, p);
    apply_impulse(a, ra, ws_vec3_scale(p, -1.0));
}
/* Hinge lock: two angular constraints that remove the child's rotation about the
 * two axes perpendicular to the hinge axis, turning the ball socket into a true
 * hinge. Without it a leg splays sideways and the free rotations resonate
 * against the planted feet. The position error is the swing component of the
 * relative rotation, measured about each perpendicular axis; the constraint is
 * driven to zero with a Baumgarte bias. */
static void solve_hinge_lock(ws_body3d *c, ws_joint3 *j)
{
    if (!j->hinge_lock) {
        return;
    }
    ws_body3 *a = &c->bodies[j->a];
    ws_body3 *b = &c->bodies[j->b];
    /* Two parent-local axes perpendicular to the hinge axis. */
    ws_vec3 p1, p2;
    if (j->hinge_axis == 0) {
        p1 = ws_vec3_make(0.0, 1.0, 0.0);
        p2 = ws_vec3_make(0.0, 0.0, 1.0);
    } else if (j->hinge_axis == 1) {
        p1 = ws_vec3_make(1.0, 0.0, 0.0);
        p2 = ws_vec3_make(0.0, 0.0, 1.0);
    } else {
        p1 = ws_vec3_make(1.0, 0.0, 0.0);
        p2 = ws_vec3_make(0.0, 1.0, 0.0);
    }
    ws_mat3 ra = ws_quat_to_mat3(a->orient);
    ws_mat3 rb = ws_quat_to_mat3(b->orient);
    /* Relative rotation R = Ra^T Rb. Its rotation vector, to first order, is the
     * antisymmetric part; its components along p1 and p2 are the swing error. */
    ws_mat3 rel = mat3_mul(mat3_transpose(ra), rb);
    ws_vec3 rv = ws_vec3_make(0.5 * (rel.m[3 * 1 + 2] - rel.m[3 * 2 + 1]),
                              0.5 * (rel.m[3 * 2 + 0] - rel.m[3 * 0 + 2]),
                              0.5 * (rel.m[3 * 0 + 1] - rel.m[3 * 1 + 0]));
    double e1 = ws_vec3_dot(rv, p1);
    double e2 = ws_vec3_dot(rv, p2);
    ws_vec3 a1 = ws_quat_rotate(a->orient, p1);
    ws_vec3 a2 = ws_quat_rotate(a->orient, p2);
    ws_mat3 ia = ws_body3_inv_inertia_world(a);
    ws_mat3 ib = ws_body3_inv_inertia_world(b);
    double bias = c->position_bias / c->dt;
    ws_vec3 axes[2] = {a1, a2};
    double errs[2] = {e1, e2};
    for (int i = 0; i < 2; ++i) {
        ws_vec3 ax = axes[i];
        double k = ws_vec3_dot(ax, ws_mat3_mul_vec3(ib, ax)) +
                   ws_vec3_dot(ax, ws_mat3_mul_vec3(ia, ax));
        if (k < WS_EPS) {
            continue;
        }
        double edot = ws_vec3_dot(ws_vec3_sub(b->omega, a->omega), ax);
        double lambda = (-bias * errs[i] - edot) / k;
        b->omega = ws_vec3_add(b->omega,
                               ws_mat3_mul_vec3(ib, ws_vec3_scale(ax, lambda)));
        a->omega = ws_vec3_sub(a->omega,
                               ws_mat3_mul_vec3(ia, ws_vec3_scale(ax, lambda)));
    }
}

static void solve_joint_limit(ws_body3d *c, ws_joint3 *j)
{
    if (!j->limit_enabled) {
        return;
    }
    ws_body3 *a = &c->bodies[j->a];
    ws_body3 *b = &c->bodies[j->b];
    double q = ws_joint3_hinge_angle(c, j) - j->rest_angle;
    ws_vec3 axis = joint_hinge_axis_world(c, j);
    double qd = ws_vec3_dot(ws_vec3_sub(b->omega, a->omega), axis);
    /* Angular effective mass about the axis for both bodies. */
    ws_mat3 ia = ws_body3_inv_inertia_world(a);
    ws_mat3 ib = ws_body3_inv_inertia_world(b);
    double kang = ws_vec3_dot(axis, ws_mat3_mul_vec3(ia, axis)) +
                  ws_vec3_dot(axis, ws_mat3_mul_vec3(ib, axis));
    if (kang < WS_EPS) {
        return;
    }
    double bias = c->position_bias / c->dt;
    double target = 0.0;
    int active = 0;
    if (q < j->min_angle) {
        target = bias * (j->min_angle - q);
        active = 1;
    } else if (q > j->max_angle) {
        target = -bias * (q - j->max_angle);
        active = 1;
    }
    if (!active) {
        return;
    }
    double lambda = (target - qd) / kang;
    if ((q < j->min_angle && lambda < 0.0) ||
        (q > j->max_angle && lambda > 0.0)) {
        return;
    }
    ws_vec3 imp = ws_vec3_scale(axis, lambda);
    b->omega = ws_vec3_add(b->omega, ws_mat3_mul_vec3(ib, imp));
    a->omega = ws_vec3_sub(a->omega, ws_mat3_mul_vec3(ia, imp));
}

/* ---------- step ---------- */

ws_result ws_body3d_step(ws_body3d *c)
{
    if (c == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const double dt = c->dt;

    /* 1. Forces: gravity plus muscle-equivalent motor torques. */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_body3 *b = &c->bodies[i];
        b->force = ws_vec3_make(0.0, -b->mass * c->gravity, 0.0);
        b->torque = b->extra_torque;
    }
    for (uint32_t i = 0; i < c->joint_count; ++i) {
        ws_joint3 *j = &c->joints[i];
        if (!j->motor_enabled) {
            continue;
        }
        ws_body3 *a = &c->bodies[j->a];
        ws_body3 *b = &c->bodies[j->b];
        double q = ws_joint3_hinge_angle(c, j) - j->rest_angle;
        ws_vec3 axis = joint_hinge_axis_world(c, j);
        double qd = ws_vec3_dot(ws_vec3_sub(b->omega, a->omega), axis);
        double t = j->motor_kp * (j->motor_target - q) - j->motor_kd * qd;
        if (t > j->motor_max_torque) {
            t = j->motor_max_torque;
        } else if (t < -j->motor_max_torque) {
            t = -j->motor_max_torque;
        }
        c->motor_work += fabs(t * qd) * dt;
        c->motor_power = fabs(t * qd);
        ws_vec3 tv = ws_vec3_scale(axis, t);
        b->torque = ws_vec3_add(b->torque, tv);
        a->torque = ws_vec3_sub(a->torque, tv);
    }

    /* 2. Integrate velocities (semi-implicit Euler). */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_body3 *b = &c->bodies[i];
        b->vel = ws_vec3_add(b->vel, ws_vec3_scale(b->force, b->inv_mass * dt));
        ws_mat3 iw = ws_body3_inv_inertia_world(b);
        ws_vec3 dw = ws_mat3_mul_vec3(iw, b->torque);
        b->omega = ws_vec3_add(b->omega, ws_vec3_scale(dw, dt));
    }

    /* 3. Contacts at the current pose. */
    build_contacts(c);
    for (uint32_t i = 0; i < c->contact_count; ++i) {
        c->contacts[i].lambda_n = 0.0;
        c->contacts[i].lambda_t1 = 0.0;
        c->contacts[i].lambda_t2 = 0.0;
    }

    /* 4. Sequential-impulse solve. */
    for (int it = 0; it < c->solver_iterations; ++it) {
        for (uint32_t i = 0; i < c->joint_count; ++i) {
            solve_joint_point(c, &c->joints[i]);
        }
        for (uint32_t i = 0; i < c->joint_count; ++i) {
            solve_hinge_lock(c, &c->joints[i]);
        }
        for (uint32_t i = 0; i < c->joint_count; ++i) {
            solve_joint_limit(c, &c->joints[i]);
        }
        for (uint32_t i = 0; i < c->contact_count; ++i) {
            solve_contact(c, &c->contacts[i]);
        }
    }

    /* 5. Integrate positions and orientations, then correct foot penetration. */
    for (uint32_t i = 0; i < c->body_count; ++i) {
        ws_body3 *b = &c->bodies[i];
        b->pos = ws_vec3_add(b->pos, ws_vec3_scale(b->vel, dt));
        b->orient = ws_quat_integrate(b->orient, b->omega, dt);
    }
    if (c->contact_count > 0) {
        ws_vec3 dx = ws_vec3_make(0.0, 0.0, 0.0);
        for (uint32_t i = 0; i < c->contact_count; ++i) {
            const ws_contact3 *ct = &c->contacts[i];
            if (ct->penetration <= 0.0) {
                continue;
            }
            dx = ws_vec3_add(dx, ws_vec3_scale(ct->normal, ct->penetration));
        }
        if (dx.x != 0.0 || dx.y != 0.0 || dx.z != 0.0) {
            for (uint32_t i = 0; i < c->body_count; ++i) {
                c->bodies[i].pos = ws_vec3_add(c->bodies[i].pos, dx);
            }
        }
    }
    return WS_OK;
}

ws_result ws_body3d_advance(ws_body3d *c, uint64_t steps)
{
    if (c == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    for (uint64_t s = 0; s < steps; ++s) {
        ws_result r = ws_body3d_step(c);
        if (r != WS_OK) {
            return r;
        }
    }
    return WS_OK;
}

/* ---------- aggregate queries ---------- */

double ws_body3d_total_mass(const ws_body3d *c)
{
    double m = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        m += c->bodies[i].mass;
    }
    return m;
}

ws_vec3 ws_body3d_center_of_mass(const ws_body3d *c)
{
    double m = 0.0;
    ws_vec3 s = ws_vec3_make(0.0, 0.0, 0.0);
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_body3 *b = &c->bodies[i];
        m += b->mass;
        s = ws_vec3_add(s, ws_vec3_scale(b->pos, b->mass));
    }
    if (m > 0.0) {
        return ws_vec3_scale(s, 1.0 / m);
    }
    return ws_vec3_make(0.0, 0.0, 0.0);
}

double ws_body3d_kinetic_energy(const ws_body3d *c)
{
    double e = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_body3 *b = &c->bodies[i];
        e += 0.5 * b->mass * ws_vec3_dot(b->vel, b->vel);
        /* I_world = R * I_body * R^T; energy is 0.5 w . (I_world w). */
        ws_mat3 r = ws_quat_to_mat3(b->orient);
        ws_mat3 iw_real = mat3_mul(r, mat3_mul(b->inertia_body, mat3_transpose(r)));
        ws_vec3 iwv = ws_mat3_mul_vec3(iw_real, b->omega);
        e += 0.5 * ws_vec3_dot(b->omega, iwv);
    }
    return e;
}

ws_vec3 ws_body3d_linear_momentum(const ws_body3d *c)
{
    ws_vec3 p = ws_vec3_make(0.0, 0.0, 0.0);
    for (uint32_t i = 0; i < c->body_count; ++i) {
        p = ws_vec3_add(p, ws_vec3_scale(c->bodies[i].vel, c->bodies[i].mass));
    }
    return p;
}

ws_vec3 ws_body3d_angular_momentum(const ws_body3d *c)
{
    ws_vec3 l = ws_vec3_make(0.0, 0.0, 0.0);
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_body3 *b = &c->bodies[i];
        ws_vec3 orbital = ws_vec3_cross(b->pos, ws_vec3_scale(b->vel, b->mass));
        ws_mat3 r = ws_quat_to_mat3(b->orient);
        ws_mat3 iw_real = mat3_mul(r, mat3_mul(b->inertia_body, mat3_transpose(r)));
        ws_vec3 spin = ws_mat3_mul_vec3(iw_real, b->omega);
        l = ws_vec3_add(l, ws_vec3_add(orbital, spin));
    }
    return l;
}
