#include "serializer/serializer.h"

static GenResult gen_emit_props(const GenValue *props, GenStrBuf *buf)
{
    if (props == NULL) {
        if (!gen_strbuf_append_char(buf, '\n')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    }
    if (!gen_strbuf_append_char(buf, ' ')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (gen_value_format_impl(props, 0, buf) != GEN_OK) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (!gen_strbuf_append_char(buf, '\n')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

GenResult gen_serialize_document(const GenDocument *document, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    size_t i;
    GenResult rc;

    if (document == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    if (out_length != NULL) {
        *out_length = 0;
    }
    gen_strbuf_init(&buf);

    for (i = 0; i < document->types_order.count; i++) {
        GenTypeNode *t = (GenTypeNode *)document->types_order.items[i];
        const char *kw = t->kind == GEN_TYPE_KIND_GENUS ? "cins" : "tur";
        if (t->parent != NULL) {
            if (!gen_strbuf_appendf(&buf, "%s %s -> %s", kw, t->name, t->parent->name)) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else {
            if (!gen_strbuf_appendf(&buf, "%s %s", kw, t->name)) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        rc = gen_emit_props(t->props, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }

    if (document->types_order.count > 0 &&
        (document->sets_order.count > 0 || document->entities_order.count > 0) &&
        !gen_strbuf_append_char(&buf, '\n')) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < document->sets_order.count; i++) {
        GenSetNode *s = (GenSetNode *)document->sets_order.items[i];
        if (!gen_strbuf_appendf(&buf, "kume %s", s->name)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = gen_emit_props(s->props, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }

    if (document->sets_order.count > 0 && document->entities_order.count > 0 &&
        !gen_strbuf_append_char(&buf, '\n')) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < document->entities_order.count; i++) {
        GenEntity *e = (GenEntity *)document->entities_order.items[i];
        if (e->type != NULL) {
            if (!gen_strbuf_appendf(&buf, "veri %s : %s ", e->name, e->type->name)) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else if (e->value != NULL && e->value->kind == GEN_VALUE_OBJECT) {
            if (!gen_strbuf_appendf(&buf, "veri %s ", e->name)) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else {
            if (!gen_strbuf_appendf(&buf, "veri %s = ", e->name)) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        rc = gen_value_format_impl(e->value, 0, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&buf, "\n\n")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }

    for (i = 0; i < document->memberships.count; i++) {
        GenMembershipRec *m = (GenMembershipRec *)document->memberships.items[i];
        if (!gen_strbuf_appendf(&buf, "uye %s -> %s\n", m->entity, m->set)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }

    *out_text = gen_strbuf_steal(&buf, out_length);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

GenResult gen_document_serialize(
    const GenDocument *document,
    char **out_text,
    size_t *out_length
)
{
    return gen_serialize_document(document, out_text, out_length);
}
