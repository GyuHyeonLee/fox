internal void
OpenglRectangle(v2 minPos, v2 maxPos, v4 color)
{
   glBegin(GL_TRIANGLES);

    // NOTE : Lower Traingle
   glTexCoord2f(0.0f, 0.0f);
   glVertex2f(minPos.x, minPos.y);

   glTexCoord2f(1.0f, 0.0f);
   glVertex2f(maxPos.x, minPos.y);

   glTexCoord2f(1.0f, 1.0f);
   glVertex2f(maxPos.x, maxPos.y);

    // NOTE : Lower Triangle
   glTexCoord2f(0.0f, 0.0f);
   glVertex2f(minPos.x, minPos.y);

   glTexCoord2f(1.0f, 1.0f);
   glVertex2f(maxPos.x, maxPos.y);

   glTexCoord2f(0.0f, 1.0f);
   glVertex2f(minPos.x, maxPos.y);

   glEnd();
}

internal void
RenderGroupToOutputBuffer(render_group *renderGroup, loaded_bitmap *outputTarget)
{
    BEGIN_TIMED_BLOCK(RenderGroupToOutputBuffer);
    v2 screenDim = V2i(outputTarget->width, outputTarget->height);

    real32 pixelsToMeters = 1.0f/renderGroup->metersToPixels;

    for(uint32 baseIndex = 0;
        baseIndex < renderGroup->pushBufferSize;
        )
    {
        render_group_entry_header *header = (render_group_entry_header *)(renderGroup->pushBufferBase + baseIndex);
        void *data = (uint8 *)header + sizeof(*header);

        switch(header->type)
        {
            case RenderGroupEntryType_render_group_entry_clear:
            {
                render_group_entry_clear *entry = (render_group_entry_clear *)data;

                glClearColor(entry->color.r, entry->color.g, entry->color.b, entry->color.a);
                glClear(GL_COLOR_BUFFER_BIT);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            case RenderGroupEntryType_render_group_entry_bitmap:
            {
                render_group_entry_bitmap *entry = (render_group_entry_bitmap *)data;

                // TODO : Later remove this so that we can just use single word like P 
                entity_basis_pos_result basis = GetRenderEntityBasePoint(renderGroup, &entry->entryBasis, screenDim);

                    DrawSomethingHopefullyFast(outputTarget, basis.pos,
                        basis.scale*V2(entry->size.x, 0),
                        basis.scale*V2(0, entry->size.y),
                        entry->color,
                        entry->bitmap, 0, 0, 0, 0);

                v2 xAxis = V2(1.0f, 0.0f);
                v2 yAxis = V2(0.0f, 1.0f);

                v2 minPos = basis->pos;
                v2 maxPos = minPos + basis.scale * entry->size.x * xAxis + 
                            basis.scale * entry->size.y * yAxis;

                OpenglRectangle(minPos, maxPos, entry->color);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            case RenderGroupEntryType_render_group_entry_rectangle:
            {
                render_group_entry_rectangle *entry = (render_group_entry_rectangle *)data;

                // TODO : Later remove this so that we can just use single word like P 
                entity_basis_pos_result basis = GetRenderEntityBasePoint(renderGroup, &entry->entryBasis, screenDim);
#if 0
                glMatrixMode(GL_TEXTURE);
                glLoadIdentity();
                // Set the modelview and projection matrix
                glMatrixMode(GL_MODELVIEW_MATRIX);   
                glLoadIdentity();
                // NOTE : Projection Matrix moves the points from the world
                // TO THE UNIT SPACE(NDC)
                glMatrixMode(GL_PROJECTION_MATRIX);
                glLoadIdentity();

#endif
                OpenglRectangle(basis->pos, basis->pos + entry->dim, entry->color);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            InvalidDefaultCase;
        }
    }

    END_TIMED_BLOCK(RenderGroupToOutputBuffer);
}
