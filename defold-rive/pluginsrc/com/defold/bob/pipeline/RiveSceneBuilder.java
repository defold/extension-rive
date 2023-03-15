// Copyright 2021 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.rive.proto.Rive.RiveSceneDesc;
import com.google.protobuf.Message;

@ProtoParams(srcClass = RiveSceneDesc.class, messageClass = RiveSceneDesc.class)
@BuilderParams(name="RiveScene", inExts=".rivescene", outExt=".rivescenec")
public class RiveSceneBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        RiveSceneDesc.Builder builder = RiveSceneDesc.newBuilder();
        ProtoUtil.merge(input, builder);

        if (!builder.getScene().equals("")) {
            BuilderUtil.checkResource(this.project, input, "scene", builder.getScene());

            taskBuilder.addInput(input.getResource(builder.getScene()));
        }

        BuilderUtil.checkResource(this.project, input, "atlas", builder.getAtlas());

        taskBuilder.addInput(this.project.getResource(builder.getAtlas()).changeExt(".a.texturesetc"));
        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        RiveSceneDesc.Builder builder = RiveSceneDesc.newBuilder();
        ProtoUtil.merge(task.input(0), builder);
        builder.setScene(BuilderUtil.replaceExt(builder.getScene(), ".riv", ".rivc"));
        builder.setAtlas(BuilderUtil.replaceExt(builder.getAtlas(), ".atlas", ".a.texturesetc"));

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
