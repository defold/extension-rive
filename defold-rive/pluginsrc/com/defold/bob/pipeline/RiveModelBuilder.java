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

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.rive.proto.Rive.RiveModelDesc;

@ProtoParams(srcClass = RiveModelDesc.class, messageClass = RiveModelDesc.class)
@BuilderParams(name="RiveModel", inExts=".rivemodel", outExt=".rivemodelc")
public class RiveModelBuilder extends ProtoBuilder<RiveModelDesc.Builder> {

    @Override
    protected RiveModelDesc.Builder transform(Task<Void> task, IResource resource, RiveModelDesc.Builder builder) throws CompileExceptionError {

        if (!builder.getScene().equals("")) {
            BuilderUtil.checkResource(this.project, resource, "scene", builder.getScene());
        }
        builder.setScene(BuilderUtil.replaceExt(builder.getScene(), ".rivescene", ".rivescenec"));

        if (!builder.getMaterial().equals("")) {
            BuilderUtil.checkResource(this.project, resource, "material", builder.getMaterial());
        }
        builder.setMaterial(BuilderUtil.replaceExt(builder.getMaterial(), ".material", ".materialc"));
        return builder;
    }
}
