// Copyright (c) 2021 Robin Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import React from 'react';
import { createStyles, withStyles, WithStyles, Theme } from '@material-ui/core/styles';

import IControlViewFactory from './IControlViewFactory';
import { PiPedalModelFactory, PiPedalModel } from "./PiPedalModel";
import { PedalBoardItem } from './PedalBoard';
import PluginControlView, { ControlGroup,ControlViewCustomization } from './PluginControlView';
import ToobFrequencyResponseView from './ToobFrequencyResponseView';



const styles = (theme: Theme) => createStyles({
});

interface ToobToneStackProps extends WithStyles<typeof styles> {
    instanceId: number;
    item: PedalBoardItem;

};
interface ToobToneStackState {

};

const ToobToneStackView =
    withStyles(styles, { withTheme: true })(
        class extends React.Component<ToobToneStackProps, ToobToneStackState> 
        implements ControlViewCustomization
        {
            model: PiPedalModel;

            customizationId: number = 1; 

            constructor(props: ToobToneStackProps) {
                super(props);
                this.model = PiPedalModelFactory.getInstance();
                this.state = {
                }
            }

            ModifyControls(controls: (React.ReactNode| ControlGroup)[]): (React.ReactNode| ControlGroup)[]
            {
                controls.splice(0,0,
                    ( <ToobFrequencyResponseView instanceId={this.props.instanceId}  />)
                    );
                return controls;
            }
            render() {
                return (<PluginControlView
                    instanceId={this.props.instanceId}
                    item={this.props.item}
                    customization={this}
                    customizationId={this.customizationId}
                />);
            }
        }
    );



class ToobToneStackViewFactory implements IControlViewFactory {
    uri: string = "http://two-play.com/plugins/toob-tone-stack";

    Create(model: PiPedalModel, pedalBoardItem: PedalBoardItem): React.ReactNode {
        return (<ToobToneStackView instanceId={pedalBoardItem.instanceId} item={pedalBoardItem} />);
    }


};
export default ToobToneStackViewFactory;